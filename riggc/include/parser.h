#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "token.h"
#include <stddef.h>

typedef enum
{
  TYPE_I8,
  TYPE_I16,
  TYPE_I32,
  TYPE_I64,
  TYPE_U8,
  TYPE_U16,
  TYPE_U32,
  TYPE_U64,
  TYPE_F32,
  TYPE_F64,
  TYPE_BOOL,
  TYPE_STR,
  TYPE_PTR,
  TYPE_VOID,
  TYPE_UNKNOWN, /* sentinel: type could not be inferred */
} TypeKind;

typedef enum
{
  EXPR_INT_LIT,
  EXPR_FLOAT_LIT,
  EXPR_STR_LIT,
  EXPR_BOOL_LIT,
  EXPR_IDENT,
  EXPR_CALL,
  EXPR_QUAL_CALL,
  EXPR_UNARY,
  EXPR_BINARY,
  EXPR_INDEX,
  EXPR_ASSIGN,
  EXPR_CAST,
} ExprKind;

typedef struct Expr Expr;

typedef struct
{
  Expr **args;
  int count;
} ArgList;

struct Expr
{
  ExprKind kind;
  int line;
  union
  {
    long long ival;
    double fval;
    struct
    {
      const char *ptr;
      int len;
    } sval;
    struct
    {
      const char *ptr;
      int len;
    } ident;
    int bval;
    struct
    {
      const char *name;
      int name_len;
      ArgList args;
    } call;
    struct
    {
      const char *concept;
      int concept_len;
      const char *name;
      int name_len;
      ArgList args;
    } qual_call;
    struct
    {
      TokenKind op;
      Expr *operand;
    } unary;
    struct
    {
      TokenKind op;
      Expr *left;
      Expr *right;
    } binary;
    struct
    {
      Expr *target;
      Expr *index;
    } index;
    struct
    {
      Expr *target;
      Expr *value;
    } assign;
    struct
    {
      Expr *expr;
      TypeKind target_type;
    } cast;
  } as;
};

typedef enum
{
  STMT_LET,
  STMT_CONST,
  STMT_RETURN,
  STMT_IF,
  STMT_WHILE,
  STMT_LOOP,
  STMT_BREAK,
  STMT_CONTINUE,
  STMT_EXPR,
} StmtKind;

typedef struct Stmt Stmt;

typedef struct
{
  Stmt **stmts;
  int count;
} Block;

struct Stmt
{
  StmtKind kind;
  int line;
  union
  {
    struct
    {
      const char *name;
      int name_len;
      TypeKind type;
      int is_mut;
      Expr *init;
    } let;
    struct
    {
      const char *name;
      int name_len;
      TypeKind type;
      Expr *init;
    } konst;
    struct
    {
      Expr *value;
    } ret;
    struct
    {
      Expr *cond;
      Block then_block;
      Block else_block;
    } sif;
    struct
    {
      Expr *cond;
      Block body;
    } swhile;
    struct
    {
      Block body;
    } sloop;
    struct
    {
      Expr *expr;
    } sexpr;
  } as;
};

typedef struct
{
  const char *name;
  int name_len;
  TypeKind type;
} Param;

typedef struct
{
  const char *name;
  int name_len;
  Param *params;
  int param_count;
  TypeKind return_type;
  Block body;
  int line;
} FnDecl;

typedef enum
{
  EXTERN_FN,
  EXTERN_VAR
} ExternKind;

typedef struct
{
  ExternKind kind;
  const char *name;
  int name_len;
  Param *params;
  int param_count;
  TypeKind return_type;
  int is_variadic;
  int line;
} ExternDecl;

typedef struct
{
  FnDecl *fns;
  int fn_count;
  ExternDecl *externs;
  int extern_count;
} Program;

typedef struct
{
  int line;
  char *message;
  char code[8];
} ParseError;

typedef struct
{
  Lexer lexer;
  Token current;
  Token previous;

  ParseError *errors;
  int error_count;
  int error_cap;

  char *arena;
  size_t arena_used;
  size_t arena_cap;
  int arena_exhausted; /* set when arena runs out; parse aborts gracefully */
} Parser;

void parser_init(Parser *p, const char *src, size_t arena_size);
Program parser_run(Parser *p);
void parser_free(Parser *p);
#ifdef RIGG_DEBUG
void ast_dump(const Program *prog);
#endif

#endif
