/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include "parser.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration — arena_alloc needs to emit an error on exhaustion */
static void push_error(Parser *p, int line, const char *code, const char *fmt, ...);

static void *arena_alloc(Parser *p, size_t size)
{
  size = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
  if (p->arena_used + size > p->arena_cap)
  {
    if (!p->arena_exhausted)
    {
      p->arena_exhausted = 1;
      push_error(p, p->current.line, "S001", "parser arena exhausted — file is too large to parse");
    }
    return NULL;
  }
  void *ptr = p->arena + p->arena_used;
  p->arena_used += size;
  memset(ptr, 0, size);
  return ptr;
}

static void *safe_realloc(void *ptr, size_t size)
{
  void *p2 = realloc(ptr, size);
  if (!p2)
  {
    fprintf(stderr, "parser: out of memory\n");
    exit(1);
  }
  return p2;
}

static void push_error(Parser *p, int line, const char *code, const char *fmt, ...)
{
  if (p->error_count == p->error_cap)
  {
    p->error_cap = p->error_cap ? p->error_cap * 2 : 8;
    p->errors = safe_realloc(p->errors, p->error_cap * sizeof(ParseError));
  }
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  char *buf = malloc((size_t) n + 1);
  va_start(ap, fmt);
  vsnprintf(buf, (size_t) n + 1, fmt, ap);
  va_end(ap);
  p->errors[p->error_count].line = line;
  p->errors[p->error_count].message = buf;
  strncpy(p->errors[p->error_count].code, code, sizeof(p->errors[p->error_count].code) - 1);
  p->errors[p->error_count].code[sizeof(p->errors[p->error_count].code) - 1] = '\0';
  p->error_count++;
}

static void advance_token(Parser *p)
{
  p->previous = p->current;
  p->current = lexer_next(&p->lexer);
}

static int check(const Parser *p, TokenKind k)
{
  return p->current.kind == k;
}

static int match(Parser *p, TokenKind k)
{
  if (!check(p, k))
    return 0;
  advance_token(p);
  return 1;
}

static void report_s001(Parser *p, int line, const char *fmt, ...)
{
  if (p->recovering)
    return;

  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  char *buf = malloc((size_t) n + 1);
  va_start(ap, fmt);
  vsnprintf(buf, (size_t) n + 1, fmt, ap);
  va_end(ap);
  push_error(p, line, "S001", "%s", buf);
  free(buf);
  p->recovering = 1;
}

static int expect(Parser *p, TokenKind k, const char *what)
{
  if (match(p, k))
    return 1;
  report_s001(p, p->current.line, "expected %s but got '%.*s'", what, p->current.len,
              p->current.start);
  return 0;
}

/* Skip to the next clean statement/declaration boundary after an error. */
static void synchronise(Parser *p, int in_block)
{
  while (p->current.kind != TOK_EOF)
  {
    if (p->current.kind == TOK_RBRACE)
    {
      if (in_block)
        return;
      advance_token(p);
      return;
    }
    if (p->current.kind == TOK_FN && !in_block)
      return;
    advance_token(p);
    if (p->previous.kind == TOK_SEMI)
      return;
  }
}

static void recover(Parser *p, int in_block)
{
  if (!p->recovering)
    return;
  synchronise(p, in_block);
  p->recovering = 0;
}

static int parse_type(Parser *p, TypeKind *out)
{
  if (p->recovering)
    return 0;
  switch (p->current.kind)
  {
    case TOK_I8:
      *out = TYPE_I8;
      break;
    case TOK_I16:
      *out = TYPE_I16;
      break;
    case TOK_I32:
      *out = TYPE_I32;
      break;
    case TOK_I64:
      *out = TYPE_I64;
      break;
    case TOK_U8:
      *out = TYPE_U8;
      break;
    case TOK_U16:
      *out = TYPE_U16;
      break;
    case TOK_U32:
      *out = TYPE_U32;
      break;
    case TOK_U64:
      *out = TYPE_U64;
      break;
    case TOK_F32:
      *out = TYPE_F32;
      break;
    case TOK_F64:
      *out = TYPE_F64;
      break;
    case TOK_BOOL:
      *out = TYPE_BOOL;
      break;
    case TOK_STR:
      *out = TYPE_STR;
      break;
    case TOK_PTR:
      *out = TYPE_PTR;
      break;
    default:
      push_error(p, p->current.line, "T002", "expected a type but got '%.*s'", p->current.len,
                 p->current.start);
      return 0;
  }
  advance_token(p);
  return 1;
}

static Expr *parse_expr(Parser *p);

static Expr *make_expr(Parser *p, ExprKind kind, int line)
{
  Expr *e = arena_alloc(p, sizeof(Expr));
  if (!e)
    return NULL;
  e->kind = kind;
  e->line = line;
  return e;
}

/* Build an arg list into a temp heap buffer then copy into the arena */
static ArgList parse_arglist(Parser *p)
{
  ArgList al = {0};
  Expr **args = NULL;
  int argc = 0, argc_cap = 0;
  if (!check(p, TOK_RPAREN) && !check(p, TOK_EOF))
  {
    do
    {
      if (argc == argc_cap)
      {
        argc_cap = argc_cap ? argc_cap * 2 : 4;
        args = safe_realloc(args, argc_cap * sizeof(Expr *));
      }
      args[argc++] = parse_expr(p);
      if (p->recovering || !args[argc - 1])
        break;
    } while (match(p, TOK_COMMA));
  }
  if (!expect(p, TOK_RPAREN, "')'"))
  {
    free(args);
    return al;
  }
  if (argc)
  {
    Expr **arena_args = arena_alloc(p, argc * sizeof(Expr *));
    if (arena_args)
    {
      memcpy(arena_args, args, argc * sizeof(Expr *));
      al.args = arena_args;
    }
  }
  al.count = argc;
  free(args);
  return al;
}

static Expr *parse_primary(Parser *p)
{
  int line = p->current.line;

  if (check(p, TOK_INT_LIT))
  {
    Expr *e = make_expr(p, EXPR_INT_LIT, line);
    /* strtoll needs null-termination; token points into source */
    char buf[32];
    int len = p->current.len < 31 ? p->current.len : 31;
    memcpy(buf, p->current.start, len);
    buf[len] = '\0';
    e->as.ival = strtoll(buf, NULL, 10);
    advance_token(p);
    return e;
  }

  if (check(p, TOK_FLOAT_LIT))
  {
    Expr *e = make_expr(p, EXPR_FLOAT_LIT, line);
    char buf[64];
    int len = p->current.len < 63 ? p->current.len : 63;
    memcpy(buf, p->current.start, len);
    buf[len] = '\0';
    e->as.fval = strtod(buf, NULL);
    advance_token(p);
    return e;
  }

  if (check(p, TOK_STR_LIT))
  {
    Expr *e = make_expr(p, EXPR_STR_LIT, line);
    e->as.sval.ptr = p->current.start;
    e->as.sval.len = p->current.len;
    advance_token(p);
    return e;
  }

  if (match(p, TOK_TRUE))
  {
    Expr *e = make_expr(p, EXPR_BOOL_LIT, line);
    e->as.bval = 1;
    return e;
  }
  if (match(p, TOK_FALSE))
  {
    Expr *e = make_expr(p, EXPR_BOOL_LIT, line);
    e->as.bval = 0;
    return e;
  }

  if (match(p, TOK_LPAREN))
  {
    Expr *e = parse_expr(p);
    if (!e || !expect(p, TOK_RPAREN, "')'"))
      return NULL;
    return e;
  }

  if (check(p, TOK_IDENT))
  {
    const char *name = p->current.start;
    int name_len = p->current.len;
    advance_token(p);

    if (match(p, TOK_COLON_COLON))
    {
      Expr *e = make_expr(p, EXPR_QUAL_CALL, line);
      e->as.qual_call.concept = name;
      e->as.qual_call.concept_len = name_len;
      e->as.qual_call.name = p->current.start;
      e->as.qual_call.name_len = p->current.len;
      if (!expect(p, TOK_IDENT, "function name after '::'"))
        return NULL;
      if (!expect(p, TOK_LPAREN, "'('"))
        return NULL;
      e->as.qual_call.args = parse_arglist(p);
      if (p->recovering)
        return NULL;
      return e;
    }

    Expr *e = make_expr(p, EXPR_IDENT, line);
    e->as.ident.ptr = name;
    e->as.ident.len = name_len;
    return e;
  }

  report_s001(p, line, "expected expression but got '%.*s'", p->current.len, p->current.start);
  return NULL;
}

static Expr *parse_postfix(Parser *p)
{
  Expr *e = parse_primary(p);
  while (e)
  {
    if (match(p, TOK_LPAREN))
    {
      if (e->kind != EXPR_IDENT)
      {
        report_s001(p, e->line, "expected identifier before '('");
        return NULL;
      }
      Expr *call = make_expr(p, EXPR_CALL, e->line);
      call->as.call.name = e->as.ident.ptr;
      call->as.call.name_len = e->as.ident.len;
      call->as.call.args = parse_arglist(p);
      if (p->recovering)
        return NULL;
      e = call;
    }
    else if (match(p, TOK_LBRACKET))
    {
      int line = p->previous.line;
      Expr *idx = make_expr(p, EXPR_INDEX, line);
      idx->as.index.target = e;
      idx->as.index.index = parse_expr(p);
      if (!expect(p, TOK_RBRACKET, "']'"))
        return NULL;
      e = idx;
    }
    else if (match(p, TOK_PLUS_PLUS) || match(p, TOK_MINUS_MINUS))
    {
      int line = p->previous.line;
      Expr *upd = make_expr(p, EXPR_UPDATE, line);
      upd->as.update.target = e;
      upd->as.update.op = p->previous.kind;
      e = upd;
      break;
    }
    else
      break;
  }
  return e;
}

static Expr *parse_assign(Parser *p)
{
  Expr *e = parse_postfix(p);
  if (e && match(p, TOK_ASSIGN))
  {
    int line = p->previous.line;
    Expr *a = make_expr(p, EXPR_ASSIGN, line);
    a->as.assign.target = e;
    a->as.assign.value = parse_expr(p);
    if (p->recovering)
      return NULL;
    return a;
  }
  return e;
}

static Expr *parse_unary(Parser *p)
{
  int line = p->current.line;
  if (check(p, TOK_BANG) || check(p, TOK_MINUS))
  {
    TokenKind op = p->current.kind;
    advance_token(p);
    Expr *e = make_expr(p, EXPR_UNARY, line);
    e->as.unary.op = op;
    e->as.unary.operand = parse_unary(p);
    return e;
  }
  return parse_assign(p);
}

static int binary_precedence(TokenKind k)
{
  switch (k)
  {
    case TOK_OR:
      return 1;
    case TOK_AND:
      return 2;
    case TOK_EQ:
    case TOK_NEQ:
      return 3;
    case TOK_LT:
    case TOK_GT:
    case TOK_LTE:
    case TOK_GTE:
      return 4;
    case TOK_PLUS:
    case TOK_MINUS:
      return 5;
    case TOK_STAR:
    case TOK_SLASH:
    case TOK_PERCENT:
      return 6;
    default:
      return -1;
  }
}

/* Precedence climbing. prec+1 on the recursive call gives left-associativity:
   a - b - c  →  (a - b) - c, not  a - (b - c). */
static Expr *parse_binary(Parser *p, int min_prec)
{
  Expr *left = parse_unary(p);
  if (!left)
    return NULL;

  while (1)
  {
    int prec = binary_precedence(p->current.kind);
    if (prec < min_prec)
      break;
    int line = p->current.line;
    TokenKind op = p->current.kind;
    advance_token(p);
    Expr *right = parse_binary(p, prec + 1);
    if (!right)
      return NULL;
    Expr *e = make_expr(p, EXPR_BINARY, line);
    e->as.binary.op = op;
    e->as.binary.left = left;
    e->as.binary.right = right;
    left = e;
  }
  return left;
}

static Expr *parse_cast(Parser *p)
{
  Expr *e = parse_binary(p, 0);
  while (e && match(p, TOK_AS))
  {
    int line = p->previous.line;
    TypeKind target;
    if (!parse_type(p, &target))
      return NULL;
    Expr *cast = make_expr(p, EXPR_CAST, line);
    cast->as.cast.expr = e;
    cast->as.cast.target_type = target;
    e = cast;
  }
  return e;
}

static Expr *parse_expr(Parser *p)
{
  return parse_cast(p);
}

static Block parse_block(Parser *p);
static Stmt *parse_stmt(Parser *p);

static Stmt *make_stmt(Parser *p, StmtKind kind, int line)
{
  Stmt *s = arena_alloc(p, sizeof(Stmt));
  if (!s)
    return NULL;
  s->kind = kind;
  s->line = line;
  return s;
}

static Stmt *parse_let(Parser *p)
{
  int line = p->current.line;
  if (!expect(p, TOK_LET, "'let'"))
    return NULL;
  Stmt *s = make_stmt(p, STMT_LET, line);
  s->as.let.is_mut = match(p, TOK_MUT);
  s->as.let.name = p->current.start;
  s->as.let.name_len = p->current.len;
  if (!expect(p, TOK_IDENT, "variable name"))
    return NULL;
  if (!expect(p, TOK_COLON, "':'"))
    return NULL;
  if (!parse_type(p, &s->as.let.type))
    return NULL;
  if (!expect(p, TOK_ASSIGN, "'='"))
    return NULL;
  s->as.let.init = parse_expr(p);
  if (!expect(p, TOK_SEMI, "';'"))
    return NULL;
  return s;
}

static Stmt *parse_const(Parser *p)
{
  int line = p->current.line;
  if (!expect(p, TOK_CONST, "'const'"))
    return NULL;
  Stmt *s = make_stmt(p, STMT_CONST, line);
  s->as.konst.name = p->current.start;
  s->as.konst.name_len = p->current.len;
  if (!expect(p, TOK_IDENT, "constant name"))
    return NULL;
  if (!expect(p, TOK_COLON, "':'"))
    return NULL;
  if (!parse_type(p, &s->as.konst.type))
    return NULL;
  if (!expect(p, TOK_ASSIGN, "'='"))
    return NULL;
  s->as.konst.init = parse_expr(p);
  if (!expect(p, TOK_SEMI, "';'"))
    return NULL;
  return s;
}

static Stmt *parse_return(Parser *p)
{
  int line = p->current.line;
  if (!expect(p, TOK_RETURN, "'return'"))
    return NULL;
  Stmt *s = make_stmt(p, STMT_RETURN, line);
  if (!check(p, TOK_SEMI))
    s->as.ret.value = parse_expr(p);
  if (!expect(p, TOK_SEMI, "';'"))
    return NULL;
  return s;
}

static Stmt *parse_if(Parser *p)
{
  int line = p->current.line;
  if (!expect(p, TOK_IF, "'if'"))
    return NULL;
  Stmt *s = make_stmt(p, STMT_IF, line);
  s->as.sif.cond = parse_expr(p);
  if (!s->as.sif.cond && p->recovering)
    return NULL;
  s->as.sif.then_block = parse_block(p);
  if (match(p, TOK_ELSE))
  {
    if (check(p, TOK_IF))
    {
      /* else if — wrap nested if as single-statement else block */
      Stmt *nested = parse_if(p);
      if (!nested && p->recovering)
        return NULL;
      Stmt **arena_s = arena_alloc(p, sizeof(Stmt *));
      arena_s[0] = nested;
      s->as.sif.else_block.stmts = arena_s;
      s->as.sif.else_block.count = 1;
    }
    else
    {
      s->as.sif.else_block = parse_block(p);
    }
  }
  return s;
}

static Stmt *parse_while(Parser *p)
{
  int line = p->current.line;
  if (!expect(p, TOK_WHILE, "'while'"))
    return NULL;
  Stmt *s = make_stmt(p, STMT_WHILE, line);
  s->as.swhile.cond = parse_expr(p);
  if (!s->as.swhile.cond && p->recovering)
    return NULL;
  s->as.swhile.body = parse_block(p);
  return s;
}

static Stmt *parse_for(Parser *p)
{
  int line = p->current.line;
  if (!expect(p, TOK_FOR, "'for'"))
    return NULL;
  Stmt *s = make_stmt(p, STMT_FOR, line);
  if (!expect(p, TOK_LPAREN, "'('"))
    return NULL;
  s->as.sfor.init = parse_let(p);
  if (!s->as.sfor.init && p->recovering)
    return NULL;
  s->as.sfor.cond = parse_expr(p);
  if (!s->as.sfor.cond && p->recovering)
    return NULL;
  if (!expect(p, TOK_SEMI, "';'"))
    return NULL;
  s->as.sfor.post = parse_expr(p);
  if (!s->as.sfor.post && p->recovering)
    return NULL;
  if (!expect(p, TOK_RPAREN, "')'"))
    return NULL;
  s->as.sfor.body = parse_block(p);
  return s;
}

static Stmt *parse_loop(Parser *p)
{
  int line = p->current.line;
  if (!expect(p, TOK_LOOP, "'loop'"))
    return NULL;
  Stmt *s = make_stmt(p, STMT_LOOP, line);
  s->as.sloop.body = parse_block(p);
  return s;
}

static Stmt *parse_stmt(Parser *p)
{
  int line = p->current.line;
  switch (p->current.kind)
  {
    case TOK_LET:
      return parse_let(p);
    case TOK_CONST:
      return parse_const(p);
    case TOK_RETURN:
      return parse_return(p);
    case TOK_IF:
      return parse_if(p);
    case TOK_FOR:
      return parse_for(p);
    case TOK_WHILE:
      return parse_while(p);
    case TOK_LOOP:
      return parse_loop(p);
    case TOK_BREAK:
    {
      Stmt *s = make_stmt(p, STMT_BREAK, line);
      advance_token(p);
      if (!expect(p, TOK_SEMI, "';'"))
        return NULL;
      return s;
    }
    case TOK_CONTINUE:
    {
      Stmt *s = make_stmt(p, STMT_CONTINUE, line);
      advance_token(p);
      if (!expect(p, TOK_SEMI, "';'"))
        return NULL;
      return s;
    }
    default:
    {
      Stmt *s = make_stmt(p, STMT_EXPR, line);
      s->as.sexpr.expr = parse_expr(p);
      if (!s->as.sexpr.expr)
        return NULL;
      if (!expect(p, TOK_SEMI, "';'"))
        return NULL;
      return s;
    }
  }
}

static Block parse_block(Parser *p)
{
  Block b = {0};
  if (!expect(p, TOK_LBRACE, "'{'"))
  {
    recover(p, 1);
    return b;
  }
  Stmt **stmts = NULL;
  int count = 0, cap = 0;
  while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF))
  {
    Stmt *s = parse_stmt(p);
    if (!s)
    {
      recover(p, 1);
      continue;
    }
    if (count == cap)
    {
      cap = cap ? cap * 2 : 8;
      stmts = safe_realloc(stmts, cap * sizeof(Stmt *));
    }
    stmts[count++] = s;
  }
  if (!expect(p, TOK_RBRACE, "'}'"))
  {
    free(stmts);
    recover(p, 1);
    return b;
  }
  if (count)
  {
    Stmt **arena_stmts = arena_alloc(p, count * sizeof(Stmt *));
    if (arena_stmts)
      memcpy(arena_stmts, stmts, count * sizeof(Stmt *));
    b.stmts = arena_stmts;
  }
  b.count = count;
  free(stmts);
  return b;
}

static FnDecl parse_fn(Parser *p)
{
  FnDecl f = {0};
  if (!expect(p, TOK_FN, "'fn'"))
    return f;
  /* Capture line after consuming 'fn' so it points at the function name */
  f.line = p->current.line;
  f.name = p->current.start;
  f.name_len = p->current.len;
  if (!expect(p, TOK_IDENT, "function name"))
    return f;
  if (!expect(p, TOK_LPAREN, "'('"))
    return f;

  Param *params = NULL;
  int pcount = 0, pcap = 0;
  if (!check(p, TOK_RPAREN) && !check(p, TOK_EOF))
  {
    do
    {
      if (pcount == pcap)
      {
        pcap = pcap ? pcap * 2 : 4;
        params = safe_realloc(params, pcap * sizeof(Param));
      }
      Param pm = {0};
      pm.name = p->current.start;
      pm.name_len = p->current.len;
      if (!expect(p, TOK_IDENT, "parameter name"))
        break;
      if (!expect(p, TOK_COLON, "':'"))
        break;
      if (!parse_type(p, &pm.type))
      {
        free(params);
        return f;
      }
      params[pcount++] = pm;
    } while (match(p, TOK_COMMA));
  }
  if (!expect(p, TOK_RPAREN, "')'"))
  {
    free(params);
    return f;
  }

  if (pcount)
  {
    Param *arena_params = arena_alloc(p, pcount * sizeof(Param));
    if (arena_params)
      memcpy(arena_params, params, pcount * sizeof(Param));
    f.params = arena_params;
  }
  f.param_count = pcount;

  f.return_type = TYPE_VOID;
  if (match(p, TOK_ARROW))
  {
    if (!parse_type(p, &f.return_type))
    {
      free(params);
      return f;
    }
  }

  f.body = parse_block(p);
  free(params);
  return f;
}

static ExternDecl parse_extern(Parser *p)
{
  ExternDecl e = {0};
  if (!expect(p, TOK_EXTERN, "'extern'"))
    return e;
  e.line = p->current.line;

  if (match(p, TOK_VAR))
  {
    e.kind = EXTERN_VAR;
    e.name = p->current.start;
    e.name_len = p->current.len;
    if (!expect(p, TOK_IDENT, "variable name"))
      return e;
    if (!expect(p, TOK_COLON, "':'"))
      return e;
    if (!parse_type(p, &e.return_type))
      return e;
    if (!expect(p, TOK_SEMI, "';'"))
      return e;
    return e;
  }

  if (!expect(p, TOK_FN, "'fn'"))
    return e;
  e.kind = EXTERN_FN;
  e.name = p->current.start;
  e.name_len = p->current.len;
  if (!expect(p, TOK_IDENT, "function name"))
    return e;
  if (!expect(p, TOK_LPAREN, "'('"))
    return e;

  Param *params = NULL;
  int pcount = 0, pcap = 0;
  while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF))
  {
    if (check(p, TOK_ELLIPSIS))
    {
      advance_token(p);
      e.is_variadic = 1;
      break;
    }
    if (pcount == pcap)
    {
      pcap = pcap ? pcap * 2 : 4;
      params = safe_realloc(params, pcap * sizeof(Param));
    }
    Param pm = {0};
    pm.name = p->current.start;
    pm.name_len = p->current.len;
    if (!expect(p, TOK_IDENT, "parameter name"))
      break;
    if (!expect(p, TOK_COLON, "':'"))
      break;
    if (!parse_type(p, &pm.type))
    {
      free(params);
      return e;
    }
    params[pcount++] = pm;
    if (!check(p, TOK_RPAREN) && !check(p, TOK_ELLIPSIS))
    {
      if (!expect(p, TOK_COMMA, "','"))
        break;
    }
    else
    {
      match(p, TOK_COMMA);
    }
  }
  if (!expect(p, TOK_RPAREN, "')'"))
  {
    free(params);
    return e;
  }

  if (pcount)
  {
    Param *arena_params = arena_alloc(p, pcount * sizeof(Param));
    if (arena_params)
      memcpy(arena_params, params, pcount * sizeof(Param));
    e.params = arena_params;
  }
  e.param_count = pcount;

  e.return_type = TYPE_VOID;
  if (match(p, TOK_ARROW))
  {
    if (!parse_type(p, &e.return_type))
    {
      free(params);
      return e;
    }
  }

  if (!expect(p, TOK_SEMI, "';'"))
  {
    free(params);
    return e;
  }
  free(params);
  return e;
}

void parser_init(Parser *p, const char *src, size_t arena_size)
{
  memset(p, 0, sizeof(*p));
  lexer_init(&p->lexer, src);
  p->arena = malloc(arena_size);
  p->arena_cap = arena_size;
  advance_token(p);
}

Program parser_run(Parser *p)
{
  Program prog = {0};
  FnDecl *fns = NULL;
  int fcount = 0, fcap = 0;
  ExternDecl *externs = NULL;
  int ecount = 0, ecap = 0;

  while (!check(p, TOK_EOF))
  {
    if (check(p, TOK_EXTERN))
    {
      int start_errors = p->error_count;
      if (ecount == ecap)
      {
        ecap = ecap ? ecap * 2 : 4;
        externs = safe_realloc(externs, ecap * sizeof(ExternDecl));
      }
      ExternDecl e = parse_extern(p);
      if (p->error_count == start_errors)
        externs[ecount++] = e;
      recover(p, 0);
      continue;
    }
    if (!check(p, TOK_FN))
    {
      report_s001(p, p->current.line, "expected 'fn' or 'extern' at top level but got '%.*s'",
                  p->current.len, p->current.start);
      recover(p, 0);
      continue;
    }
    if (fcount == fcap)
    {
      fcap = fcap ? fcap * 2 : 8;
      fns = safe_realloc(fns, fcap * sizeof(FnDecl));
    }
    int start_errors = p->error_count;
    FnDecl f = parse_fn(p);
    if (p->error_count == start_errors)
      fns[fcount++] = f;
    recover(p, 0);
  }

  if (fcount)
  {
    FnDecl *arena_fns = arena_alloc(p, fcount * sizeof(FnDecl));
    if (arena_fns)
      memcpy(arena_fns, fns, fcount * sizeof(FnDecl));
    prog.fns = arena_fns;
  }
  prog.fn_count = fcount;
  free(fns);

  if (ecount)
  {
    ExternDecl *arena_externs = arena_alloc(p, ecount * sizeof(ExternDecl));
    if (arena_externs)
      memcpy(arena_externs, externs, ecount * sizeof(ExternDecl));
    prog.externs = arena_externs;
  }
  prog.extern_count = ecount;
  free(externs);

  return prog;
}

void parser_free(Parser *p)
{
  for (int i = 0; i < p->error_count; i++)
    free(p->errors[i].message);
  free(p->errors);
  free(p->arena);
}

static const char *type_name(TypeKind t)
{
  switch (t)
  {
    case TYPE_I8:
      return "i8";
    case TYPE_I16:
      return "i16";
    case TYPE_I32:
      return "i32";
    case TYPE_I64:
      return "i64";
    case TYPE_U8:
      return "u8";
    case TYPE_U16:
      return "u16";
    case TYPE_U32:
      return "u32";
    case TYPE_U64:
      return "u64";
    case TYPE_F32:
      return "f32";
    case TYPE_F64:
      return "f64";
    case TYPE_BOOL:
      return "bool";
    case TYPE_STR:
      return "str";
    case TYPE_PTR:
      return "ptr";
    case TYPE_VOID:
      return "void";
    default:
      return "?";
  }
}

static const char *op_name(TokenKind k)
{
  switch (k)
  {
    case TOK_PLUS:
      return "+";
    case TOK_PLUS_PLUS:
      return "++";
    case TOK_MINUS:
      return "-";
    case TOK_MINUS_MINUS:
      return "--";
    case TOK_STAR:
      return "*";
    case TOK_SLASH:
      return "/";
    case TOK_PERCENT:
      return "%";
    case TOK_EQ:
      return "==";
    case TOK_NEQ:
      return "!=";
    case TOK_LT:
      return "<";
    case TOK_GT:
      return ">";
    case TOK_LTE:
      return "<=";
    case TOK_GTE:
      return ">=";
    case TOK_AND:
      return "&&";
    case TOK_OR:
      return "||";
    case TOK_BANG:
      return "!";
    default:
      return "?";
  }
}

static void do_indent(int depth)
{
  for (int i = 0; i < depth * 2; i++)
    putchar(' ');
}

static void dump_expr(const Expr *e, int depth)
{
  if (!e)
  {
    do_indent(depth);
    printf("(null)\n");
    return;
  }
  do_indent(depth);
  switch (e->kind)
  {
    case EXPR_INT_LIT:
      printf("IntLit %lld\n", e->as.ival);
      break;
    case EXPR_FLOAT_LIT:
      printf("FloatLit %g\n", e->as.fval);
      break;
    case EXPR_STR_LIT:
      printf("StrLit \"%.*s\"\n", e->as.sval.len, e->as.sval.ptr);
      break;
    case EXPR_BOOL_LIT:
      printf("BoolLit %s\n", e->as.bval ? "true" : "false");
      break;
    case EXPR_IDENT:
      printf("Ident %.*s\n", e->as.sval.len, e->as.sval.ptr);
      break;
    case EXPR_CALL:
      printf("Call %.*s\n", e->as.call.name_len, e->as.call.name);
      for (int i = 0; i < e->as.call.args.count; i++)
        dump_expr(e->as.call.args.args[i], depth + 1);
      break;
    case EXPR_QUAL_CALL:
      printf("QualCall %.*s::%.*s\n", e->as.qual_call.concept_len, e->as.qual_call.concept,
             e->as.qual_call.name_len, e->as.qual_call.name);
      for (int i = 0; i < e->as.qual_call.args.count; i++)
        dump_expr(e->as.qual_call.args.args[i], depth + 1);
      break;
    case EXPR_UNARY:
      printf("Unary %s\n", op_name(e->as.unary.op));
      dump_expr(e->as.unary.operand, depth + 1);
      break;
    case EXPR_BINARY:
      printf("Binary %s\n", op_name(e->as.binary.op));
      dump_expr(e->as.binary.left, depth + 1);
      dump_expr(e->as.binary.right, depth + 1);
      break;
    case EXPR_INDEX:
      printf("Index\n");
      dump_expr(e->as.index.target, depth + 1);
      dump_expr(e->as.index.index, depth + 1);
      break;
    case EXPR_ASSIGN:
      printf("Assign\n");
      dump_expr(e->as.assign.target, depth + 1);
      dump_expr(e->as.assign.value, depth + 1);
      break;
    case EXPR_UPDATE:
      printf("Update %s\n", op_name(e->as.update.op));
      dump_expr(e->as.update.target, depth + 1);
      break;
    case EXPR_CAST:
      printf("Cast %s\n", type_name(e->as.cast.target_type));
      dump_expr(e->as.cast.expr, depth + 1);
      break;
  }
}

static void dump_block(const Block *b, int depth);

static void dump_stmt(const Stmt *s, int depth)
{
  if (!s)
    return;
  do_indent(depth);
  switch (s->kind)
  {
    case STMT_LET:
      printf("Let%s %.*s: %s\n", s->as.let.is_mut ? " mut" : "", s->as.let.name_len, s->as.let.name,
             type_name(s->as.let.type));
      dump_expr(s->as.let.init, depth + 1);
      break;
    case STMT_CONST:
      printf("Const %.*s: %s\n", s->as.konst.name_len, s->as.konst.name,
             type_name(s->as.konst.type));
      dump_expr(s->as.konst.init, depth + 1);
      break;
    case STMT_RETURN:
      printf("Return\n");
      if (s->as.ret.value)
        dump_expr(s->as.ret.value, depth + 1);
      break;
    case STMT_IF:
      printf("If\n");
      do_indent(depth + 1);
      printf("Cond\n");
      dump_expr(s->as.sif.cond, depth + 2);
      do_indent(depth + 1);
      printf("Then\n");
      dump_block(&s->as.sif.then_block, depth + 2);
      if (s->as.sif.else_block.count)
      {
        do_indent(depth + 1);
        printf("Else\n");
        dump_block(&s->as.sif.else_block, depth + 2);
      }
      break;
    case STMT_FOR:
      printf("For\n");
      do_indent(depth + 1);
      printf("Init\n");
      dump_stmt(s->as.sfor.init, depth + 2);
      do_indent(depth + 1);
      printf("Cond\n");
      dump_expr(s->as.sfor.cond, depth + 2);
      do_indent(depth + 1);
      printf("Post\n");
      dump_expr(s->as.sfor.post, depth + 2);
      do_indent(depth + 1);
      printf("Body\n");
      dump_block(&s->as.sfor.body, depth + 2);
      break;
    case STMT_WHILE:
      printf("While\n");
      do_indent(depth + 1);
      printf("Cond\n");
      dump_expr(s->as.swhile.cond, depth + 2);
      do_indent(depth + 1);
      printf("Body\n");
      dump_block(&s->as.swhile.body, depth + 2);
      break;
    case STMT_LOOP:
      printf("Loop\n");
      dump_block(&s->as.sloop.body, depth + 1);
      break;
    case STMT_BREAK:
      printf("Break\n");
      break;
    case STMT_CONTINUE:
      printf("Continue\n");
      break;
    case STMT_EXPR:
      printf("ExprStmt\n");
      dump_expr(s->as.sexpr.expr, depth + 1);
      break;
  }
}

static void dump_block(const Block *b, int depth)
{
  for (int i = 0; i < b->count; i++)
    dump_stmt(b->stmts[i], depth);
}

#ifdef RIGG_DEBUG
void ast_dump(const Program *prog)
{
  printf("Program\n");
  for (int i = 0; i < prog->fn_count; i++)
  {
    const FnDecl *f = &prog->fns[i];
    printf("  Fn %.*s(", f->name_len, f->name);
    for (int j = 0; j < f->param_count; j++)
    {
      if (j)
        printf(", ");
      printf("%.*s: %s", f->params[j].name_len, f->params[j].name, type_name(f->params[j].type));
    }
    printf(") -> %s\n", type_name(f->return_type));
    dump_block(&f->body, 2);
  }
}
#endif /* RIGG_DEBUG */
