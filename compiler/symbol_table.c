// symbol_table.c — Symbol Table Builder
// yeh module AST walk karke saare variables aur functions ko track karta hai
// naam, type, scope, line number, aur references count store hota hai

#include "symbol_table.h"
#include <string.h>
#include <stdio.h>

static SymTable *g_st;

// symbol table me variable ka naam dhoondho — mila to index, nahi to -1
static int find_symbol(const char *name) {
    for (int i = 0; i < g_st->count; i++)
        if (strcmp(g_st->entries[i].name, name) == 0) return i;
    return -1;
}

// naya variable/function declaration add karo symbol table me
static void add_declaration(const char *name, const char *type,
                            const char *scope, int line) {
    int idx = find_symbol(name);
    if (idx >= 0) return; /* already declared */
    if (g_st->count >= MAX_SYMBOLS_ST) return;
    SymEntry *e = &g_st->entries[g_st->count++];
    strncpy(e->name,  name,  SYM_NAME_LEN  - 1); e->name[SYM_NAME_LEN  - 1] = '\0';
    strncpy(e->type,  type,  SYM_TYPE_LEN  - 1); e->type[SYM_TYPE_LEN  - 1] = '\0';
    strncpy(e->scope, scope, SYM_SCOPE_LEN - 1); e->scope[SYM_SCOPE_LEN - 1] = '\0';
    e->line = line;
    e->references = 0;
}

static void inc_ref(const char *name) {//Jab variable use hota hai:
    int idx = find_symbol(name);
    if (idx >= 0) g_st->entries[idx].references++;
}

// AST recursively walk karo aur declarations/references collect karo
static void walk(const ASTNode *node, const char *scope) {
    if (!node) return;

    if (strcmp(node->type, "VarDecl") == 0) {
        /* value = "type name" */
        char vtype[64] = "", vname[128] = "";
        sscanf(node->value, "%63s %127s", vtype, vname);
        add_declaration(vname, vtype, scope, node->line);
        /* Walk expression children */
        for (int i = 0; i < node->child_count; i++) walk(node->children[i], scope);
        return;
    }

    if (strcmp(node->type, "FuncDecl") == 0) {
        add_declaration(node->value, "func", "global", node->line);
        char func_scope[SYM_SCOPE_LEN];
        snprintf(func_scope, sizeof(func_scope), "func %s", node->value);
        for (int i = 0; i < node->child_count; i++) {
            ASTNode *child = node->children[i];
            if (strcmp(child->type, "Param") == 0) {
                char ptype[64] = "", pname[128] = "";
                sscanf(child->value, "%63s %127s", ptype, pname);
                add_declaration(pname, ptype, func_scope, child->line);
            } else {
                walk(child, func_scope);
            }
        }
        return;
    }

    if (strcmp(node->type, "Identifier") == 0) {
        inc_ref(node->value);
        return;
    }

    if (strcmp(node->type, "Assignment") == 0) {
        inc_ref(node->value);
        for (int i = 0; i < node->child_count; i++) walk(node->children[i], scope);
        return;
    }

    for (int i = 0; i < node->child_count; i++) walk(node->children[i], scope);
}

// yeh function baahar se call hota hai — AST root se symbol table build karta hai
SymTable symbol_table_build(const ASTNode *root) {
    SymTable st;
    memset(&st, 0, sizeof(st));
    g_st = &st;
    walk(root, "global");
    g_st = NULL;
    return st;
}

// symbol table ko JSON array format me convert karta hai
void symbol_table_to_json(const SymTable *st, StrBuf *out) {
    buf_append(out, "[");
    for (int i = 0; i < st->count; i++) {
        if (i > 0) buf_append(out, ",");
        const SymEntry *e = &st->entries[i];
        buf_append(out, "{");
        buf_append(out, "\"name\":");        buf_append_json_str(out, e->name);
        buf_append(out, ",\"type\":");       buf_append_json_str(out, e->type);
        buf_append(out, ",\"scope\":");      buf_append_json_str(out, e->scope);
        buf_appendf(out, ",\"line\":%d",     e->line);
        buf_appendf(out, ",\"references\":%d", e->references);
        buf_append(out, "}");
    }
    buf_append(out, "]");
}
