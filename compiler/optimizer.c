// optimizer.c — Code Optimizer Module
// yeh module AST aur source code par safe optimizations apply karta hai:
// Constant Propagation, Constant Folding, Dead Code Elimination, Unused Variable, Loop Invariant

#include "optimizer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Line splitting — source code ko lines me todna ── */
#define MAX_LINES 65536
static char *g_lines[MAX_LINES];         /* pointers into modified copy */
static char  g_line_buf[1024 * 1024];    /* working buffer for opt lines — matches MAX_INPUT */
static int   g_line_count;

static void init_lines(const char *src) {
    strncpy(g_line_buf, src, sizeof(g_line_buf) - 1);
    g_line_buf[sizeof(g_line_buf) - 1] = '\0';
    g_line_count = 0;
    char *p = g_line_buf;
    g_lines[g_line_count++] = p;
    while (*p && g_line_count < MAX_LINES) {
        if (*p == '\n') {
            *p = '\0';
            g_lines[g_line_count++] = p + 1;
        }
        p++;
    }
}

static void join_lines(char *out, int outlen) {
    out[0] = '\0';
    for (int i = 0; i < g_line_count; i++) {
        if (g_lines[i] == NULL) continue;
        strncat(out, g_lines[i], (size_t)outlen - strlen(out) - 1);
        if (i < g_line_count - 1)
            strncat(out, "\n", (size_t)outlen - strlen(out) - 1);
    }
}

/* ── Helper: BinaryOp node ko evaluate karne ki koshish karo ───── */
static int try_eval(const ASTNode *node, double *result) {
    if (!node || strcmp(node->type, "BinaryOp") != 0) return 0;
    if (node->child_count < 2) return 0;
    const ASTNode *L = node->children[0];
    const ASTNode *R = node->children[1];
    if (strcmp(L->type, "Literal") != 0 || strcmp(R->type, "Literal") != 0) return 0;
    /* Check not string */
    if (L->value[0] == '"' || R->value[0] == '"') return 0;
    char *ep = NULL;
    double lv = strtod(L->value, &ep); if (ep == L->value) return 0;
    double rv = strtod(R->value, &ep); if (ep == R->value) return 0;
    const char *op = node->value;
    if (strcmp(op, "+") == 0) { *result = lv + rv; return 1; }
    if (strcmp(op, "-") == 0) { *result = lv - rv; return 1; }
    if (strcmp(op, "*") == 0) { *result = lv * rv; return 1; }
    if (strcmp(op, "/") == 0) {
        if (rv == 0.0) return 0;
        *result = lv / rv; return 1;
    }
    return 0;
}

static const char *op_name(const char *op) {
    if (strcmp(op, "+") == 0) return "addition";
    if (strcmp(op, "-") == 0) return "subtraction";
    if (strcmp(op, "*") == 0) return "multiplication";
    if (strcmp(op, "/") == 0) return "division";
    return "operation";
}

/* ── Constant folding — compile-time par expression evaluate karna ── */
static OptResult *g_opt;

// yeh function constant expressions ko compile-time par fold karta hai
static void const_fold(const ASTNode *node) {
    if (!node) return;
    if (strcmp(node->type, "VarDecl") == 0 && node->child_count == 1) {
        const ASTNode *expr = node->children[0];
        double val = 0.0;
        if (try_eval(expr, &val) && g_opt->opt_count < MAX_OPTS) {
            int lidx = node->line - 1;
            if (lidx >= 0 && lidx < g_line_count && g_lines[lidx]) {
                char result_str[64];
                if (val == (double)(int)val)
                    snprintf(result_str, sizeof(result_str), "%d", (int)val);
                else
                    snprintf(result_str, sizeof(result_str), "%g", val);

                /* Build new line */
                char vtype[64] = "", vname[64] = "";
                sscanf(node->value, "%63s %63s", vtype, vname);
                char new_line[512];
                /* Preserve leading whitespace */
                const char *orig = g_lines[lidx];
                int lead = 0;
                while (orig[lead] == ' ' || orig[lead] == '\t') lead++;
                snprintf(new_line, sizeof(new_line), "%.*s%s %s = %s;",
                         lead, orig, vtype, vname, result_str);

                OptEntry *oe = &g_opt->opts[g_opt->opt_count++];
                strncpy(oe->type, "Constant Folding", sizeof(oe->type) - 1);
                oe->line = node->line;
                strncpy(oe->original,  orig, OPT_STR_LEN - 1);
                strncpy(oe->optimized, new_line, OPT_STR_LEN - 1);
                const char *lv = expr->child_count > 0 ? expr->children[0]->value : "?";
                const char *rv = expr->child_count > 1 ? expr->children[1]->value : "?";
                snprintf(oe->explanation, OPT_STR_LEN,
                         "The expression %s %s %s is evaluated at compile time to produce %s, "
                         "eliminating the %s at runtime.",
                         lv, expr->value, rv, result_str, op_name(expr->value));

                g_lines[lidx] = NULL; /* temporary: will replace */
                /* We store new line in a static buffer pool — use opt entry */
                g_lines[lidx] = oe->optimized;
            }
        }
    }
    for (int i = 0; i < node->child_count; i++) const_fold(node->children[i]);
}

/* ── Dead code elimination — return ke baad ka code hatana ── */
// yeh function return statement ke baad aane wale dead code ko remove karta hai
static void dead_code_elim(const ASTNode *node) {
    if (!node) return;
    if (strcmp(node->type, "Block") == 0) {
        int found_ret = 0, ret_line = 0;
        for (int i = 0; i < node->child_count && g_opt->opt_count < MAX_OPTS; i++) {
            if (found_ret) {
                int dead_line = node->children[i]->line;
                if (dead_line > 0) {
                    int lidx = dead_line - 1;
                    if (lidx >= 0 && lidx < g_line_count && g_lines[lidx]) {
                        OptEntry *oe = &g_opt->opts[g_opt->opt_count++];
                        strncpy(oe->type, "Dead Code Elimination", sizeof(oe->type) - 1);
                        oe->line = dead_line;
                        strncpy(oe->original,  g_lines[lidx], OPT_STR_LEN - 1);
                        strncpy(oe->optimized, "(removed)",    OPT_STR_LEN - 1);
                        snprintf(oe->explanation, OPT_STR_LEN,
                                 "This statement on line %d appears after the return statement on "
                                 "line %d and will never execute. Removing it reduces code size.",
                                 dead_line, ret_line);
                        g_lines[lidx] = NULL; /* mark as removed */
                    }
                }
            }
            if (strcmp(node->children[i]->type, "ReturnStmt") == 0) {
                found_ret = 1;
                ret_line  = node->children[i]->line;
            }
        }
    }
    for (int i = 0; i < node->child_count; i++) dead_code_elim(node->children[i]);
}

/* ── Unused variable detection — jo variable use nahi hua usko hataana ── */
/* declarations collect karo */
static void collect_decls(const ASTNode *node, char names[64][64], int lines[64], int *cnt) {
    if (!node) return;
    if (strcmp(node->type, "VarDecl") == 0) {
        char vtype[64] = "", vname[64] = "";
        sscanf(node->value, "%63s %63s", vtype, vname);
        if (*cnt < 64) {
            strncpy(names[*cnt], vname, 63); names[*cnt][63] = '\0';
            lines[*cnt] = node->line;
            (*cnt)++;
        }
    }
    for (int i = 0; i < node->child_count; i++) collect_decls(node->children[i], names, lines, cnt);
}

static void collect_used(const ASTNode *node, char used[64][64], int *ucnt) {
    if (!node) return;
    if (strcmp(node->type, "Identifier") == 0 || strcmp(node->type, "Assignment") == 0) {
        int found = 0;
        for (int k = 0; k < *ucnt; k++) if (strcmp(used[k], node->value) == 0) { found = 1; break; }
        if (!found && *ucnt < 64) { strncpy(used[(*ucnt)], node->value, 63); used[(*ucnt)++][63] = '\0'; }
    }
    for (int i = 0; i < node->child_count; i++) collect_used(node->children[i], used, ucnt);
}

// yeh function check karta hai ki koi variable declared hai par used nahi
static void unused_var_check(const ASTNode *root) {
    char decl_names[64][64]; int decl_lines[64]; int dcnt = 0;
    char used_names[64][64]; int ucnt = 0;
    collect_decls(root, decl_names, decl_lines, &dcnt);
    collect_used(root, used_names, &ucnt);
    for (int d = 0; d < dcnt && g_opt->opt_count < MAX_OPTS; d++) {
        int used = 0;
        for (int u = 0; u < ucnt; u++) if (strcmp(used_names[u], decl_names[d]) == 0) { used = 1; break; }
        if (!used) {
            OptEntry *oe = &g_opt->opts[g_opt->opt_count++];
            strncpy(oe->type, "Unused Variable", sizeof(oe->type) - 1);
            oe->line = decl_lines[d];

            /* Use the actual source line as 'original' if available */
            int lidx = decl_lines[d] - 1;
            if (lidx >= 0 && lidx < g_line_count && g_lines[lidx]) {
                strncpy(oe->original, g_lines[lidx], OPT_STR_LEN - 1);
                g_lines[lidx] = NULL;  /* ← actually remove from optimized output */
            } else {
                snprintf(oe->original, OPT_STR_LEN, "(declaration of %s)", decl_names[d]);
            }

            strncpy(oe->optimized, "(removed — unused)", OPT_STR_LEN - 1);
            snprintf(oe->explanation, OPT_STR_LEN,
                     "Variable '%s' is declared on line %d but never read anywhere in the code. "
                     "The declaration has been removed in the optimized output.",
                     decl_names[d], decl_lines[d]);
        }
    }
}


/* ── Loop invariant ─────────────────────────── */
static void expr_to_str(const ASTNode *node, char *out, int outlen) {
    if (!node || outlen <= 0) return;
    if (strcmp(node->type,"Literal")==0 || strcmp(node->type,"Identifier")==0) {
        strncpy(out, node->value, (size_t)outlen - 1); out[outlen-1] = '\0'; return;
    }
    if (strcmp(node->type,"BinaryOp")==0 && node->child_count >= 2) {
        char l[128]="", r[128]="";
        expr_to_str(node->children[0], l, sizeof(l));
        expr_to_str(node->children[1], r, sizeof(r));
        snprintf(out, (size_t)outlen, "%s %s %s", l, node->value, r);
        return;
    }
    out[0]='\0';
}

static void get_modified_vars(const ASTNode *node, char mods[64][64], int *mcnt) {
    if (!node) return;
    if (strcmp(node->type,"Assignment")==0) {
        int f=0; for(int k=0;k<*mcnt;k++) if(strcmp(mods[k],node->value)==0){f=1;break;}
        if(!f && *mcnt<64){ strncpy(mods[*mcnt],node->value,63); mods[(*mcnt)++][63]='\0'; }
    }
    if (strcmp(node->type,"VarDecl")==0) {
        char vtype[64]="",vname[64]=""; sscanf(node->value,"%63s %63s",vtype,vname);
        int f=0; for(int k=0;k<*mcnt;k++) if(strcmp(mods[k],vname)==0){f=1;break;}
        if(!f && *mcnt<64){ strncpy(mods[*mcnt],vname,63); mods[(*mcnt)++][63]='\0'; }
    }
    for(int i=0;i<node->child_count;i++) get_modified_vars(node->children[i],mods,mcnt);
}

static void get_expr_vars(const ASTNode *node, char ev[16][64], int *ecnt) {
    if (!node) return;
    if (strcmp(node->type,"Identifier")==0) {
        int f=0; for(int k=0;k<*ecnt;k++) if(strcmp(ev[k],node->value)==0){f=1;break;}
        if(!f && *ecnt<16){ strncpy(ev[*ecnt],node->value,63); ev[(*ecnt)++][63]='\0'; }
    }
    for(int i=0;i<node->child_count;i++) get_expr_vars(node->children[i],ev,ecnt);
}

static void check_invariant_exprs(const ASTNode *node,
        char mods[64][64], int mcnt) {
    if (!node) return;
    if (strcmp(node->type,"BinaryOp")==0 && g_opt->opt_count < MAX_OPTS) {
        char ev[16][64]; int ecnt=0;
        get_expr_vars(node, ev, &ecnt);
        if (ecnt > 0) {
            int modified = 0;
            for(int e=0;e<ecnt;e++)
                for(int m=0;m<mcnt;m++)
                    if(strcmp(ev[e],mods[m])==0){ modified=1; break; }
            if (!modified) {
                char estr[256]="";
                expr_to_str(node, estr, sizeof(estr));
                if (strlen(estr) > 2) {
                    OptEntry *oe = &g_opt->opts[g_opt->opt_count++];
                    strncpy(oe->type, "Loop Invariant", sizeof(oe->type)-1);
                    oe->line = node->line;
                    strncpy(oe->original,  estr, OPT_STR_LEN-1);
                    snprintf(oe->optimized, OPT_STR_LEN, "(move \"%s\" before loop)", estr);
                    snprintf(oe->explanation, OPT_STR_LEN,
                             "The expression '%s' does not depend on any variable modified inside "
                             "the loop. It can be computed once before the loop.", estr);
                    return; /* don't recurse */
                }
            }
        }
    }
    for(int i=0;i<node->child_count;i++) check_invariant_exprs(node->children[i], mods, mcnt);
}

// yeh function loop ke andar invariant expressions dhundh ke report karta hai
static void loop_invariant(const ASTNode *node) {
    if (!node) return;
    if (strcmp(node->type,"WhileStmt")==0 || strcmp(node->type,"ForStmt")==0) {
        char mods[64][64]; int mcnt=0;
        get_modified_vars(node, mods, &mcnt);
        check_invariant_exprs(node, mods, mcnt);
    }
    for(int i=0;i<node->child_count;i++) loop_invariant(node->children[i]);
}

/* ── Constant Propagation — variable ko uski known constant value se replace karna ── */

#define MAX_CONST_MAP 128

// yeh struct ek variable ka naam aur uski constant value store karta hai
typedef struct {
    char name[64];
    char value[128];
    int  is_valid;   // 1 = constant value known hai, 0 = unknown
} ConstMapEntry;

static ConstMapEntry g_const_map[MAX_CONST_MAP];
static int g_const_map_count;

// constant map me variable dhoondho
static int cmap_find(const char *name) {
    for (int i = 0; i < g_const_map_count; i++)
        if (g_const_map[i].is_valid && strcmp(g_const_map[i].name, name) == 0) return i;
    return -1;
}

// constant map me variable add ya update karo
static void cmap_set(const char *name, const char *value) {
    int idx = cmap_find(name);
    if (idx >= 0) {
        strncpy(g_const_map[idx].value, value, 127);
        g_const_map[idx].value[127] = '\0';
        g_const_map[idx].is_valid = 1;
        return;
    }
    if (g_const_map_count >= MAX_CONST_MAP) return;
    ConstMapEntry *e = &g_const_map[g_const_map_count++];
    strncpy(e->name, name, 63); e->name[63] = '\0';
    strncpy(e->value, value, 127); e->value[127] = '\0';
    e->is_valid = 1;
}

// constant map se variable remove karo (value unknown ho gayi)
static void cmap_invalidate(const char *name) {
    int idx = cmap_find(name);
    if (idx >= 0) g_const_map[idx].is_valid = 0;
}

// check karo ki expression ek simple literal hai ya nahi
static int is_simple_literal(const ASTNode *node) {
    if (!node) return 0;
    if (strcmp(node->type, "Literal") == 0) {
        if (node->value[0] == '"') return 0; // strings skip karo
        return 1;
    }
    return 0;
}

// constant propagation — yeh function AST walk karta hai aur known constants replace karta hai
static void const_prop(const ASTNode *node) {
    if (!node) return;

    // VarDecl: agar literal assign hua hai to constant map me daalo
    if (strcmp(node->type, "VarDecl") == 0) {
        char vtype[64] = "", vname[64] = "";
        sscanf(node->value, "%63s %63s", vtype, vname);
        if (node->child_count == 1 && is_simple_literal(node->children[0])) {
            cmap_set(vname, node->children[0]->value);
        } else if (node->child_count == 1) {
            // non-literal expression — agar expression me known variables hain to replace karo
            cmap_invalidate(vname); // value unknown for now
        }
    }

    // Assignment: variable reassign ho raha hai
    if (strcmp(node->type, "Assignment") == 0) {
        if (node->child_count == 1 && is_simple_literal(node->children[0])) {
            cmap_set(node->value, node->children[0]->value);
        } else {
            cmap_invalidate(node->value); // non-constant reassignment
        }
    }

    // Identifier in expression: agar constant map me hai to source line me replace karo
    if (strcmp(node->type, "Identifier") == 0 && g_opt->opt_count < MAX_OPTS) {
        int idx = cmap_find(node->value);
        if (idx >= 0) {
            int lidx = node->line - 1;
            if (lidx >= 0 && lidx < g_line_count && g_lines[lidx]) {
                // check karo ki yeh identifier declaration me nahi hai (VarDecl ka naam skip karo)
                // source line me variable naam dhoondho aur replace karo
                char *line_str = g_lines[lidx];
                char *pos = strstr(line_str, node->value);
                if (pos) {
                    // make sure yeh '=' ke right side pe hai (simple heuristic)
                    char *eq = strchr(line_str, '=');
                    if (eq && pos > eq) {
                        // optimization entry banao
                        OptEntry *oe = &g_opt->opts[g_opt->opt_count];
                        strncpy(oe->original, line_str, OPT_STR_LEN - 1);
                        oe->original[OPT_STR_LEN - 1] = '\0';

                        // naya line banao jisme variable ki jagah constant ho
                        static char prop_lines[MAX_OPTS][512];
                        int pi = g_opt->opt_count;
                        char *new_line = prop_lines[pi];
                        int pre_len = (int)(pos - line_str);
                        int name_len = (int)strlen(node->value);
                        int val_len = (int)strlen(g_const_map[idx].value);
                        int rest_len = (int)strlen(pos + name_len);

                        if (pre_len + val_len + rest_len < 511) {
                            memcpy(new_line, line_str, (size_t)pre_len);
                            memcpy(new_line + pre_len, g_const_map[idx].value, (size_t)val_len);
                            memcpy(new_line + pre_len + val_len, pos + name_len, (size_t)rest_len);
                            new_line[pre_len + val_len + rest_len] = '\0';

                            strncpy(oe->type, "Constant Propagation", sizeof(oe->type) - 1);
                            oe->line = node->line;
                            strncpy(oe->optimized, new_line, OPT_STR_LEN - 1);
                            snprintf(oe->explanation, OPT_STR_LEN,
                                     "Replaced variable '%s' with its known constant value %s",
                                     node->value, g_const_map[idx].value);

                            g_lines[lidx] = new_line;
                            g_opt->opt_count++;
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < node->child_count; i++) const_prop(node->children[i]);
}

/* ── Public API — optimizer run karo ──────────── */
// yeh function saari optimizations sequentially apply karta hai
OptResult optimizer_run(const ASTNode *root, const char *source_code) {
    OptResult res;
    memset(&res, 0, sizeof(res));
    if (!source_code) source_code = "";
    strncpy(res.original_code, source_code, sizeof(res.original_code) - 1);

    g_opt = &res;
    init_lines(source_code);

    // constant map initialize karo
    memset(g_const_map, 0, sizeof(g_const_map));
    g_const_map_count = 0;

    // pehle constant propagation, fir folding — taaki propagated values fold ho sakein
    const_prop(root);
    const_fold(root);
    dead_code_elim(root);
    unused_var_check(root);
    loop_invariant(root);

    join_lines(res.optimized_code, sizeof(res.optimized_code));
    g_opt = NULL;
    return res;
}

// yeh function optimizer result ko JSON format me convert karta hai
void optimizer_to_json(const OptResult *res, StrBuf *out) {
    buf_append(out, "{");
    buf_append(out, "\"original_code\":");  buf_append_json_str(out, res->original_code);
    buf_append(out, ",\"optimized_code\":"); buf_append_json_str(out, res->optimized_code);
    buf_append(out, ",\"optimizations_applied\":[");
    for (int i = 0; i < res->opt_count; i++) {
        if (i > 0) buf_append(out, ",");
        const OptEntry *oe = &res->opts[i];
        buf_append(out, "{");
        buf_append(out, "\"type\":");        buf_append_json_str(out, oe->type);
        buf_appendf(out, ",\"line\":%d",     oe->line);
        buf_append(out, ",\"original\":");   buf_append_json_str(out, oe->original);
        buf_append(out, ",\"optimized\":");  buf_append_json_str(out, oe->optimized);
        buf_append(out, ",\"explanation\":"); buf_append_json_str(out, oe->explanation);
        buf_append(out, "}");
    }
    buf_append(out, "]}");
}
