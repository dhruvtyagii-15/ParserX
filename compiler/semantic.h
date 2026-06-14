#ifndef SEMANTIC_H
#define SEMANTIC_H

// yeh header file semantic analysis ke liye hai
// type checking, undeclared variable, duplicate declaration errors handle karta
// hai

#include "ast.h"
#include "json_output.h"

#define MAX_SEM_ERRORS 64
#define SEM_ERR_LEN 512

/* ── ek semantic error entry ─────────────────── */
typedef struct {
  char message[SEM_ERR_LEN]; // error ka description
  int line;                  // kis line par error aaya
} SemError;

/* ── semantic analysis ka result ─────────────── */
typedef struct {
  SemError errors[MAX_SEM_ERRORS];
  int error_count;               // kitne errors mile
  int has_error;                 // 1 agar koi bhi error ho
  char first_error[SEM_ERR_LEN]; // pehla error message (fatal stop ke liye)
} SemanticResult;

/* AST walk karke type checking karega — pehla error milne par ruk jaata hai */
SemanticResult semantic_check(const ASTNode *root);

/* Semantic errors ko JSON me convert karega */
void semantic_to_json(const SemanticResult *sr, StrBuf *out);

#endif /* SEMANTIC_H */
