// semantic.c — Semantic Analysis Module (Type Checking)
// yeh module AST walk karke type mismatches, undeclared variables,
// duplicate declarations, aur invalid operations detect karta hai

#include "semantic.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

/* ── Internal type map — variable naam se uska declared type store karta hai ── */
#define MAX_TYPE_ENTRIES 256
#define TYPE_NAME_LEN    128
#define TYPE_TYPE_LEN    64

// yeh struct ek variable ka naam aur uska type store karta hai
typedef struct {
    char name[TYPE_NAME_LEN];
    char type[TYPE_TYPE_LEN];   // "int", "float", "string"
    int  line;                  // declaration ki line
} TypeEntry;

// yeh poora type map hai — saare declared variables yahan track hote hain
typedef struct {
    TypeEntry entries[MAX_TYPE_ENTRIES];
    int       count;
} TypeMap;

/* ── Semantic checker context ── */
// yeh context struct hai jo checker walk ke dauran use hota hai
typedef struct {
    TypeMap         tmap;
    SemanticResult *result;
} SemCtx;

/* ── type map me variable dhoondho ── */
// variable ka naam deke uska index return karta hai, nahi mila to -1
static int tmap_find(const TypeMap *m, const char *name) {
    for (int i = 0; i < m->count; i++)
        if (strcmp(m->entries[i].name, name) == 0) return i;
    return -1;
}

/* ── type map me naya variable add karo ── */
// variable ka naam, type, aur line number deke map me add karta hai
static void tmap_add(TypeMap *m, const char *name, const char *type, int line) {
    if (m->count >= MAX_TYPE_ENTRIES) return;
    TypeEntry *e = &m->entries[m->count++];
    strncpy(e->name, name, TYPE_NAME_LEN - 1);  e->name[TYPE_NAME_LEN - 1] = '\0';
    strncpy(e->type, type, TYPE_TYPE_LEN - 1);  e->type[TYPE_TYPE_LEN - 1] = '\0';
    e->line = line;
}

/* ── error add karo result me ── */
// semantic error log karta hai — pehle error ko first_error me bhi store karta hai
static void sem_error(SemCtx *ctx, int line, const char *fmt, ...) {
    SemanticResult *sr = ctx->result;
    if (sr->error_count >= MAX_SEM_ERRORS) return;

    char msg[SEM_ERR_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    SemError *se = &sr->errors[sr->error_count++];
    snprintf(se->message, SEM_ERR_LEN, "%s", msg);
    se->line = line;
    sr->has_error = 1;

    // pehla error hamesha fatal error hota hai — isi ko report karte hain
    if (sr->error_count == 1) {
        snprintf(sr->first_error, SEM_ERR_LEN, "%s", msg);
    }
}

/* ── check karo ki literal string hai ya nahi ── */
// literal value agar double quotes se start ho to string hai
static int is_string_literal(const char *val) {
    return (val && val[0] == '"');
}

/* ── check karo ki literal float hai ya nahi ── */
// value me decimal point ho to float hai
static int is_float_literal(const char *val) {
    if (!val) return 0;
    for (const char *p = val; *p; p++) {
        if (*p == '.') return 1;
    }
    return 0;
}

/* ── expression ka type calculate karo — yeh recursion se kaam karta hai ── */
// AST node deke uska resulting type return karta hai: "int", "float", "string", ya "unknown"
static const char *infer_expr_type(SemCtx *ctx, const ASTNode *node) {
    if (!node) return "unknown";

    // Literal node — value dekh ke type decide karo
    if (strcmp(node->type, "Literal") == 0) {
        if (is_string_literal(node->value)) return "string";
        if (is_float_literal(node->value))  return "float";
        return "int";
    }

    // Identifier node — type map se variable ka type fetch karo
    if (strcmp(node->type, "Identifier") == 0) {
        int idx = tmap_find(&ctx->tmap, node->value);
        if (idx < 0) {
            // variable declare hi nahi hua — undeclared variable error
            sem_error(ctx, node->line,
                      "Undeclared variable at line %d: variable '%s' is not declared",
                      node->line, node->value);
            return "unknown";
        }
        return ctx->tmap.entries[idx].type;
    }

    // FuncCall node — abhi ke liye int return karta hai (function return type tracking nahi hai)
    if (strcmp(node->type, "FuncCall") == 0) {
        return "int";
    }

    // BinaryOp node — dono children ka type check karo aur result type calculate karo
    if (strcmp(node->type, "BinaryOp") == 0 && node->child_count >= 2) {
        const char *left_type  = infer_expr_type(ctx, node->children[0]);
        const char *right_type = infer_expr_type(ctx, node->children[1]);

        // agar koi bhi unknown hai to propagate karo (pehle error aa chuka hoga)
        if (strcmp(left_type, "unknown") == 0 || strcmp(right_type, "unknown") == 0)
            return "unknown";

        // string ke saath arithmetic operation allowed nahi hai
        if (strcmp(left_type, "string") == 0 || strcmp(right_type, "string") == 0) {
            sem_error(ctx, node->line,
                      "Invalid operation at line %d: cannot perform '%s' on %s and %s",
                      node->line, node->value, left_type, right_type);
            return "unknown";
        }

        // ── Expression type rules ──
        // int + int   = int
        // int + float = float
        // float + int = float
        // float + float = float
        if (strcmp(left_type, "float") == 0 || strcmp(right_type, "float") == 0)
            return "float";
        return "int";
    }

    // Condition node — condition ka type comparison hota hai, int return karte hain
    if (strcmp(node->type, "Condition") == 0) {
        // condition ke children ka type check — side effect se errors aayenge
        if (node->child_count >= 1) infer_expr_type(ctx, node->children[0]);
        if (node->child_count >= 2) infer_expr_type(ctx, node->children[1]);
        return "int";
    }

    return "unknown";
}

/* ── AST recursively walk karo aur type checking karo ── */
// yeh function har AST node ko visit karta hai aur declarations/assignments check karta hai
static void sem_walk(SemCtx *ctx, const ASTNode *node) {
    if (!node) return;

    // pehla error milte hi ruk jaao — fatal error approach
    if (ctx->result->has_error) return;

    // ── VarDecl: variable declaration handle karo ──
    // value format: "type name" e.g. "int x"
    if (strcmp(node->type, "VarDecl") == 0) {
        char vtype[64] = "", vname[128] = "";
        sscanf(node->value, "%63s %127s", vtype, vname);

        // duplicate declaration check — same naam ka variable pehle se hai?
        int idx = tmap_find(&ctx->tmap, vname);
        if (idx >= 0) {
            sem_error(ctx, node->line,
                      "Duplicate declaration at line %d: variable '%s' is already declared at line %d",
                      node->line, vname, ctx->tmap.entries[idx].line);
            return;
        }

        // type map me register karo
        tmap_add(&ctx->tmap, vname, vtype, node->line);

        // agar initializer expression hai to uska type check karo
        if (node->child_count == 1) {
            const char *expr_type = infer_expr_type(ctx, node->children[0]);

            if (ctx->result->has_error) return; // infer me error aa gaya

            if (strcmp(expr_type, "unknown") != 0) {
                // ── Assignment type rules ──
                // int = int        ✅
                // float = float    ✅
                // string = string  ✅
                // float = int      ✅ (implicit promotion)
                // int = float      ❌
                // int = string     ❌
                // float = string   ❌
                // string = int     ❌

                int compatible = 0;
                if (strcmp(vtype, expr_type) == 0) {
                    compatible = 1;  // same type — always ok
                } else if (strcmp(vtype, "float") == 0 && strcmp(expr_type, "int") == 0) {
                    compatible = 1;  // float = int allowed (promotion)
                }

                if (!compatible) {
                    sem_error(ctx, node->line,
                              "Type mismatch at line %d: cannot assign %s to %s variable '%s'",
                              node->line, expr_type, vtype, vname);
                    return;
                }
            }
        }
        return;
    }

    // ── Assignment: reassignment handle karo ──
    if (strcmp(node->type, "Assignment") == 0) {
        const char *vname = node->value;

        // check karo ki variable declared hai ya nahi
        int idx = tmap_find(&ctx->tmap, vname);
        if (idx < 0) {
            sem_error(ctx, node->line,
                      "Undeclared variable at line %d: variable '%s' is not declared",
                      node->line, vname);
            return;
        }

        // RHS expression ka type check karo
        if (node->child_count >= 1) {
            const char *expr_type = infer_expr_type(ctx, node->children[0]);

            if (ctx->result->has_error) return;

            if (strcmp(expr_type, "unknown") != 0) {
                const char *var_type = ctx->tmap.entries[idx].type;

                int compatible = 0;
                if (strcmp(var_type, expr_type) == 0) {
                    compatible = 1;
                } else if (strcmp(var_type, "float") == 0 && strcmp(expr_type, "int") == 0) {
                    compatible = 1;  // float = int allowed
                }

                if (!compatible) {
                    sem_error(ctx, node->line,
                              "Type mismatch at line %d: cannot assign %s to %s variable '%s'",
                              node->line, expr_type, var_type, vname);
                    return;
                }
            }
        }
        return;
    }

    // ── FuncDecl: function ke parameters aur body handle karo ──
    if (strcmp(node->type, "FuncDecl") == 0) {
        // function ka naam type map me "func" type se daalo
        tmap_add(&ctx->tmap, node->value, "func", node->line);

        // parameters ko register karo
        for (int i = 0; i < node->child_count; i++) {
            ASTNode *child = node->children[i];
            if (strcmp(child->type, "Param") == 0) {
                char ptype[64] = "", pname[128] = "";
                sscanf(child->value, "%63s %127s", ptype, pname);
                if (tmap_find(&ctx->tmap, pname) < 0) {
                    tmap_add(&ctx->tmap, pname, ptype, child->line);
                }
            } else {
                // function body recursively check karo
                sem_walk(ctx, child);
            }
            if (ctx->result->has_error) return;
        }
        return;
    }

    // ── PrintStmt: print ke andar ka expression check karo ──
    if (strcmp(node->type, "PrintStmt") == 0) {
        for (int i = 0; i < node->child_count; i++) {
            infer_expr_type(ctx, node->children[i]);
            if (ctx->result->has_error) return;
        }
        return;
    }

    // ── Baaki sab nodes ke children recursively check karo ──
    for (int i = 0; i < node->child_count; i++) {
        sem_walk(ctx, node->children[i]);
        if (ctx->result->has_error) return;
    }
}

/* ── Public API: semantic check run karo ── */
// yeh function baahar se call hota hai — AST root dete hain, result milta hai
SemanticResult semantic_check(const ASTNode *root) {
    SemanticResult sr;
    memset(&sr, 0, sizeof(sr));

    if (!root) return sr;

    SemCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.result = &sr;

    // AST walk start karo root se
    sem_walk(&ctx, root);

    return sr;
}

/* ── Semantic errors ko JSON array me convert karo ── */
// yeh function errors ko JSON format me likhta hai output buffer me
void semantic_to_json(const SemanticResult *sr, StrBuf *out) {
    buf_append(out, "[");
    for (int i = 0; i < sr->error_count; i++) {
        if (i > 0) buf_append(out, ",");
        buf_append(out, "{");
        buf_append(out, "\"message\":");
        buf_append_json_str(out, sr->errors[i].message);
        buf_appendf(out, ",\"line\":%d", sr->errors[i].line);
        buf_append(out, "}");
    }
    buf_append(out, "]");
}
