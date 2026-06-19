#include "parser.h"
#include "test_common.h"

#define ARENA (512 * 1024)

/* -------------------------------------------------------------------------
 * helpers
 * ---------------------------------------------------------------------- */

static Program parse(const char *src, Parser *p)
{
  parser_init(p, src, ARENA);
  return parser_run(p);
}

/* Assert no parse errors were recorded. */
static void assert_ok(const Parser *p)
{
  if (p->error_count > 0)
  {
    for (int i = 0; i < p->error_count; i++)
    {
      const ParseError *e = &p->errors[i];
      if (e->code[0])
        fprintf(stderr, "Error %s: %s\n", e->code, e->message);
      else
        fprintf(stderr, "Error: %s\n", e->message);
      if (e->line)
        fprintf(stderr, "  --> <source>:%d\n", e->line);
    }
    ASSERT(p->error_count == 0, "expected no parse errors");
  }
}

/* Assert at least one parse error was recorded. */
static void assert_has_errors(const Parser *p)
{
  ASSERT(p->error_count > 0, "expected parse errors but got none");
}

/* -------------------------------------------------------------------------
 * function declarations
 * ---------------------------------------------------------------------- */

static void test_fn_no_params_no_return(void)
{
  Parser p;
  Program prog = parse("fn main() { }", &p);
  assert_ok(&p);
  ASSERT(prog.fn_count == 1, "expected 1 function");
  ASSERT_EQ_INT(prog.fns[0].name_len, 4, "name length");
  ASSERT(memcmp(prog.fns[0].name, "main", 4) == 0, "name is 'main'");
  ASSERT_EQ_INT(prog.fns[0].param_count, 0, "no params");
  ASSERT(prog.fns[0].return_type == TYPE_VOID, "return type is void");
  parser_free(&p);
}

static void test_fn_with_return_type(void)
{
  Parser p;
  Program prog = parse("fn get() -> i32 { return 1; }", &p);
  assert_ok(&p);
  ASSERT(prog.fns[0].return_type == TYPE_I32, "return type is i32");
  parser_free(&p);
}

static void test_fn_params(void)
{
  Parser p;
  Program prog = parse("fn add(a: i32, b: i32) -> i32 { return a; }", &p);
  assert_ok(&p);
  ASSERT_EQ_INT(prog.fns[0].param_count, 2, "two params");
  ASSERT(memcmp(prog.fns[0].params[0].name, "a", 1) == 0, "first param name");
  ASSERT(prog.fns[0].params[0].type == TYPE_I32, "first param type");
  ASSERT(memcmp(prog.fns[0].params[1].name, "b", 1) == 0, "second param name");
  ASSERT(prog.fns[0].params[1].type == TYPE_I32, "second param type");
  parser_free(&p);
}

static void test_fn_all_types_as_params(void)
{
  const char *src = "fn f(a: i8, b: i16, c: i32, d: i64,"
                    "     e: u8, f: u16, g: u32, h: u64,"
                    "     i: f32, j: f64, k: bool, l: str) { }";
  Parser p;
  Program prog = parse(src, &p);
  assert_ok(&p);
  TypeKind expected[] = {TYPE_I8,  TYPE_I16, TYPE_I32, TYPE_I64, TYPE_U8,   TYPE_U16,
                         TYPE_U32, TYPE_U64, TYPE_F32, TYPE_F64, TYPE_BOOL, TYPE_STR};
  ASSERT_EQ_INT(prog.fns[0].param_count, 12, "12 params");
  for (int i = 0; i < 12; i++)
    ASSERT(prog.fns[0].params[i].type == expected[i], "param type matches");
  parser_free(&p);
}

static void test_multiple_fns(void)
{
  Parser p;
  Program prog = parse("fn a() { }\n"
                       "fn b() { }\n"
                       "fn c() { }\n",
                       &p);
  assert_ok(&p);
  ASSERT_EQ_INT(prog.fn_count, 3, "three functions");
  ASSERT(memcmp(prog.fns[0].name, "a", 1) == 0, "first fn is a");
  ASSERT(memcmp(prog.fns[1].name, "b", 1) == 0, "second fn is b");
  ASSERT(memcmp(prog.fns[2].name, "c", 1) == 0, "third fn is c");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * let / const
 * ---------------------------------------------------------------------- */

static void test_let_immutable(void)
{
  Parser p;
  Program prog = parse("fn f() { let x: i32 = 42; }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->kind == STMT_LET, "is a let");
  ASSERT(memcmp(s->as.let.name, "x", 1) == 0, "name is x");
  ASSERT(s->as.let.type == TYPE_I32, "type is i32");
  ASSERT(s->as.let.is_mut == 0, "not mutable");
  ASSERT(s->as.let.init != NULL, "has initialiser");
  parser_free(&p);
}

static void test_let_mutable(void)
{
  Parser p;
  Program prog = parse("fn f() { let mut count: i32 = 0; }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->kind == STMT_LET, "is a let");
  ASSERT(s->as.let.is_mut == 1, "is mutable");
  parser_free(&p);
}

static void test_const_decl(void)
{
  Parser p;
  Program prog = parse("fn f() { const MAX: i32 = 100; }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->kind == STMT_CONST, "is a const");
  ASSERT(memcmp(s->as.konst.name, "MAX", 3) == 0, "name is MAX");
  ASSERT(s->as.konst.type == TYPE_I32, "type is i32");
  ASSERT(s->as.konst.init != NULL, "has initialiser");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * return
 * ---------------------------------------------------------------------- */

static void test_return_value(void)
{
  Parser p;
  Program prog = parse("fn f() -> i32 { return 1; }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->kind == STMT_RETURN, "is a return");
  ASSERT(s->as.ret.value != NULL, "has return value");
  parser_free(&p);
}

static void test_return_bare(void)
{
  Parser p;
  Program prog = parse("fn f() { return; }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->kind == STMT_RETURN, "is a return");
  ASSERT(s->as.ret.value == NULL, "no return value");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * literals as expressions
 * ---------------------------------------------------------------------- */

static void test_int_literal(void)
{
  Parser p;
  Program prog = parse("fn f() { let x: i32 = 42; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.let.init;
  ASSERT(e->kind == EXPR_INT_LIT, "is int literal");
  ASSERT(e->as.ival == 42, "value is 42");
  parser_free(&p);
}

static void test_float_literal(void)
{
  Parser p;
  Program prog = parse("fn f() { let x: f64 = 3.14; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.let.init;
  ASSERT(e->kind == EXPR_FLOAT_LIT, "is float literal");
  ASSERT(e->as.fval > 3.13 && e->as.fval < 3.15, "value is ~3.14");
  parser_free(&p);
}

static void test_str_literal(void)
{
  Parser p;
  Program prog = parse("fn f() { let s: str = \"hello\"; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.let.init;
  ASSERT(e->kind == EXPR_STR_LIT, "is str literal");
  ASSERT(e->as.sval.len == 5, "length is 5");
  ASSERT(memcmp(e->as.sval.ptr, "hello", 5) == 0, "value is 'hello'");
  parser_free(&p);
}

static void test_bool_literal_true(void)
{
  Parser p;
  Program prog = parse("fn f() { let b: bool = true; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.let.init;
  ASSERT(e->kind == EXPR_BOOL_LIT, "is bool literal");
  ASSERT(e->as.bval == 1, "value is true");
  parser_free(&p);
}

static void test_bool_literal_false(void)
{
  Parser p;
  Program prog = parse("fn f() { let b: bool = false; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.let.init;
  ASSERT(e->kind == EXPR_BOOL_LIT, "is bool literal");
  ASSERT(e->as.bval == 0, "value is false");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * binary expressions
 * ---------------------------------------------------------------------- */

static void test_binary_add(void)
{
  Parser p;
  Program prog = parse("fn f() -> i32 { return a + b; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->kind == EXPR_BINARY, "is binary");
  ASSERT(e->as.binary.op == TOK_PLUS, "op is +");
  ASSERT(e->as.binary.left->kind == EXPR_IDENT, "left is ident");
  ASSERT(e->as.binary.right->kind == EXPR_IDENT, "right is ident");
  parser_free(&p);
}

static void test_binary_precedence(void)
{
  /* a + b * c  should parse as  a + (b * c) */
  Parser p;
  Program prog = parse("fn f() -> i32 { return a + b * c; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->kind == EXPR_BINARY, "root is binary");
  ASSERT(e->as.binary.op == TOK_PLUS, "root op is +");
  ASSERT(e->as.binary.right->kind == EXPR_BINARY, "right is binary");
  ASSERT(e->as.binary.right->as.binary.op == TOK_STAR, "right op is *");
  parser_free(&p);
}

static void test_binary_left_associative(void)
{
  /* a - b - c  should parse as  (a - b) - c */
  Parser p;
  Program prog = parse("fn f() -> i32 { return a - b - c; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->kind == EXPR_BINARY, "root is binary");
  ASSERT(e->as.binary.op == TOK_MINUS, "root op is -");
  ASSERT(e->as.binary.left->kind == EXPR_BINARY, "left is binary");
  ASSERT(e->as.binary.left->as.binary.op == TOK_MINUS, "left op is -");
  parser_free(&p);
}

static void test_binary_comparison(void)
{
  Parser p;
  Program prog = parse("fn f() -> bool { return x == y; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->kind == EXPR_BINARY, "is binary");
  ASSERT(e->as.binary.op == TOK_EQ, "op is ==");
  parser_free(&p);
}

static void test_binary_logical(void)
{
  Parser p;
  Program prog = parse("fn f() -> bool { return a && b || c; }", &p);
  assert_ok(&p);
  /* && binds tighter than ||, so: (a && b) || c */
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->as.binary.op == TOK_OR, "root op is ||");
  ASSERT(e->as.binary.left->as.binary.op == TOK_AND, "left op is &&");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * unary expressions
 * ---------------------------------------------------------------------- */

static void test_unary_bang(void)
{
  Parser p;
  Program prog = parse("fn f() -> bool { return !x; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->kind == EXPR_UNARY, "is unary");
  ASSERT(e->as.unary.op == TOK_BANG, "op is !");
  ASSERT(e->as.unary.operand->kind == EXPR_IDENT, "operand is ident");
  parser_free(&p);
}

static void test_unary_minus(void)
{
  Parser p;
  Program prog = parse("fn f() -> i32 { return -x; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->kind == EXPR_UNARY, "is unary");
  ASSERT(e->as.unary.op == TOK_MINUS, "op is -");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * calls
 * ---------------------------------------------------------------------- */

static void test_call_no_args(void)
{
  Parser p;
  Program prog = parse("fn f() { run(); }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.sexpr.expr;
  ASSERT(e->kind == EXPR_CALL, "is call");
  ASSERT(memcmp(e->as.call.name, "run", 3) == 0, "name is 'run'");
  ASSERT_EQ_INT(e->as.call.args.count, 0, "no args");
  parser_free(&p);
}

static void test_call_with_args(void)
{
  Parser p;
  Program prog = parse("fn f() { add(1, 2); }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.sexpr.expr;
  ASSERT(e->kind == EXPR_CALL, "is call");
  ASSERT_EQ_INT(e->as.call.args.count, 2, "two args");
  ASSERT(e->as.call.args.args[0]->kind == EXPR_INT_LIT, "first arg is int");
  ASSERT(e->as.call.args.args[1]->kind == EXPR_INT_LIT, "second arg is int");
  parser_free(&p);
}

static void test_qual_call(void)
{
  Parser p;
  Program prog = parse("fn f() { buffer::load(path); }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.sexpr.expr;
  ASSERT(e->kind == EXPR_QUAL_CALL, "is qual call");
  ASSERT(memcmp(e->as.qual_call.concept, "buffer", 6) == 0, "concept is 'buffer'");
  ASSERT(memcmp(e->as.qual_call.name, "load", 4) == 0, "fn is 'load'");
  ASSERT_EQ_INT(e->as.qual_call.args.count, 1, "one arg");
  parser_free(&p);
}

static void test_qual_call_no_args(void)
{
  Parser p;
  Program prog = parse("fn f() { renderer::draw(); }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.sexpr.expr;
  ASSERT(e->kind == EXPR_QUAL_CALL, "is qual call");
  ASSERT_EQ_INT(e->as.qual_call.args.count, 0, "no args");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * assignment
 * ---------------------------------------------------------------------- */

static void test_assign(void)
{
  Parser p;
  Program prog = parse("fn f() { x = 10; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.sexpr.expr;
  ASSERT(e->kind == EXPR_ASSIGN, "is assign");
  ASSERT(e->as.assign.target->kind == EXPR_IDENT, "target is ident");
  ASSERT(memcmp(e->as.assign.target->as.ident.ptr, "x", 1) == 0, "name is x");
  ASSERT(e->as.assign.value->kind == EXPR_INT_LIT, "value is int lit");
  parser_free(&p);
}

static void test_assign_expr_rhs(void)
{
  Parser p;
  Program prog = parse("fn f() { count = count + 1; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.sexpr.expr;
  ASSERT(e->kind == EXPR_ASSIGN, "is assign");
  ASSERT(e->as.assign.value->kind == EXPR_BINARY, "rhs is binary expr");
  parser_free(&p);
}

static void test_update_expr(void)
{
  Parser p;
  Program prog = parse("fn f() { x++; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.sexpr.expr;
  ASSERT(e->kind == EXPR_UPDATE, "is update");
  ASSERT(e->as.update.op == TOK_PLUS_PLUS, "op is ++");
  ASSERT(e->as.update.target->kind == EXPR_IDENT, "target is ident");
  parser_free(&p);
}

static void test_ptr_index_read(void)
{
  Parser p;
  Program prog = parse("fn f() -> i32 { return p[0]; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->kind == EXPR_INDEX, "is index");
  ASSERT(e->as.index.target->kind == EXPR_IDENT, "target is ident");
  ASSERT(e->as.index.index->kind == EXPR_INT_LIT, "index is int lit");
  parser_free(&p);
}

static void test_ptr_index_assign(void)
{
  Parser p;
  Program prog = parse("fn f() { p[0] = 65; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.sexpr.expr;
  ASSERT(e->kind == EXPR_ASSIGN, "is assign");
  ASSERT(e->as.assign.target->kind == EXPR_INDEX, "target is index");
  ASSERT(e->as.assign.target->as.index.index->kind == EXPR_INT_LIT, "index is int lit");
  ASSERT(e->as.assign.value->kind == EXPR_INT_LIT, "value is int lit");
  parser_free(&p);
}

static void test_cast_ptr_to_str(void)
{
  Parser p;
  Program prog = parse("fn ptr_to_str(p: ptr) -> str { return p as str; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->kind == EXPR_CAST, "is cast");
  ASSERT(e->as.cast.target_type == TYPE_STR, "casts to str");
  ASSERT(e->as.cast.expr->kind == EXPR_IDENT, "operand is ident");
  parser_free(&p);
}

static void test_cast_str_to_ptr(void)
{
  Parser p;
  Program prog = parse("fn f() { let p: ptr = s as ptr; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.let.init;
  ASSERT(e->kind == EXPR_CAST, "is cast");
  ASSERT(e->as.cast.target_type == TYPE_PTR, "casts to ptr");
  ASSERT(e->as.cast.expr->kind == EXPR_IDENT, "operand is ident");
  parser_free(&p);
}

static void test_cast_precedence(void)
{
  Parser p;
  Program prog = parse("fn f() -> str { return p + 1 as str; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->kind == EXPR_CAST, "cast binds looser than +");
  ASSERT(e->as.cast.expr->kind == EXPR_BINARY, "operand is binary +");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * grouping
 * ---------------------------------------------------------------------- */

static void test_grouped_expr(void)
{
  /* (a + b) * c — grouping overrides precedence */
  Parser p;
  Program prog = parse("fn f() -> i32 { return (a + b) * c; }", &p);
  assert_ok(&p);
  Expr *e = prog.fns[0].body.stmts[0]->as.ret.value;
  ASSERT(e->as.binary.op == TOK_STAR, "root op is *");
  ASSERT(e->as.binary.left->kind == EXPR_BINARY, "left is grouped binary");
  ASSERT(e->as.binary.left->as.binary.op == TOK_PLUS, "left op is +");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * if / else
 * ---------------------------------------------------------------------- */

static void test_if_no_else(void)
{
  Parser p;
  Program prog = parse("fn f() { if x { return; } }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->kind == STMT_IF, "is if");
  ASSERT(s->as.sif.cond != NULL, "has condition");
  ASSERT(s->as.sif.then_block.count == 1, "then has one stmt");
  ASSERT(s->as.sif.else_block.count == 0, "no else");
  parser_free(&p);
}

static void test_if_with_else(void)
{
  Parser p;
  Program prog = parse("fn f() { if x { return; } else { return; } }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->as.sif.then_block.count == 1, "then has one stmt");
  ASSERT(s->as.sif.else_block.count == 1, "else has one stmt");
  parser_free(&p);
}

static void test_if_else_if(void)
{
  Parser p;
  Program prog = parse("fn f() {\n"
                       "    if a { return; }\n"
                       "    else if b { return; }\n"
                       "    else { return; }\n"
                       "}",
                       &p);
  assert_ok(&p);
  Stmt *outer = prog.fns[0].body.stmts[0];
  ASSERT(outer->kind == STMT_IF, "outer is if");
  /* else block should contain a single nested if */
  ASSERT(outer->as.sif.else_block.count == 1, "else has one stmt");
  Stmt *inner = outer->as.sif.else_block.stmts[0];
  ASSERT(inner->kind == STMT_IF, "inner is also if");
  ASSERT(inner->as.sif.else_block.count == 1, "inner has else");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * while / loop / break / continue
 * ---------------------------------------------------------------------- */

static void test_while(void)
{
  Parser p;
  Program prog = parse("fn f() { while x < 10 { x = x + 1; } }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->kind == STMT_WHILE, "is while");
  ASSERT(s->as.swhile.cond != NULL, "has condition");
  ASSERT(s->as.swhile.body.count == 1, "body has one stmt");
  parser_free(&p);
}

static void test_loop(void)
{
  Parser p;
  Program prog = parse("fn f() { loop { break; } }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->kind == STMT_LOOP, "is loop");
  ASSERT(s->as.sloop.body.count == 1, "body has one stmt");
  ASSERT(s->as.sloop.body.stmts[0]->kind == STMT_BREAK, "body contains break");
  parser_free(&p);
}

static void test_continue(void)
{
  Parser p;
  Program prog = parse("fn f() { while true { continue; } }", &p);
  assert_ok(&p);
  Stmt *inner = prog.fns[0].body.stmts[0]->as.swhile.body.stmts[0];
  ASSERT(inner->kind == STMT_CONTINUE, "is continue");
  parser_free(&p);
}

static void test_for(void)
{
  Parser p;
  Program prog = parse("fn f() { for (let mut i: i32 = 0; i < n; i++) { break; } }", &p);
  assert_ok(&p);
  Stmt *s = prog.fns[0].body.stmts[0];
  ASSERT(s->kind == STMT_FOR, "is for");
  ASSERT(s->as.sfor.init->kind == STMT_LET, "init is let");
  ASSERT(s->as.sfor.cond->kind == EXPR_BINARY, "cond is binary");
  ASSERT(s->as.sfor.post->kind == EXPR_UPDATE, "post is update");
  ASSERT(s->as.sfor.post->as.update.op == TOK_PLUS_PLUS, "post op is ++");
  ASSERT(s->as.sfor.body.count == 1, "body has one stmt");
  ASSERT(s->as.sfor.body.stmts[0]->kind == STMT_BREAK, "body contains break");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * line numbers on nodes
 * ---------------------------------------------------------------------- */

static void test_fn_line_number(void)
{
  Parser p;
  Program prog = parse("\n\nfn f() { }", &p);
  assert_ok(&p);
  ASSERT(prog.fns[0].line == 3, "fn is on line 3");
  parser_free(&p);
}

static void test_stmt_line_number(void)
{
  Parser p;
  Program prog = parse("fn f() {\n    return 1;\n}", &p);
  assert_ok(&p);
  ASSERT(prog.fns[0].body.stmts[0]->line == 2, "return is on line 2");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * error recovery
 * ---------------------------------------------------------------------- */

static void test_error_on_top_level_non_fn(void)
{
  Parser p;
  parse("let x: i32 = 1;", &p);
  assert_has_errors(&p);
  parser_free(&p);
}

static void test_error_missing_brace(void)
{
  Parser p;
  parse("fn f() { return 1; ", &p);
  assert_has_errors(&p);
  parser_free(&p);
}

static void test_error_recovers_second_fn(void)
{
  /* first fn has a bad body, second fn should still parse */
  Parser p;
  Program prog = parse("fn bad() { let = ; }\n"
                       "fn good() { return; }\n",
                       &p);
  assert_has_errors(&p);
  /* recovery should get us to 'good' */
  int found_good = 0;
  for (int i = 0; i < prog.fn_count; i++)
  {
    if (prog.fns[i].name_len == 4 && memcmp(prog.fns[i].name, "good", 4) == 0)
      found_good = 1;
  }
  ASSERT(found_good, "recovered and parsed second fn");
  parser_free(&p);
}

static void test_empty_input(void)
{
  Parser p;
  Program prog = parse("", &p);
  assert_ok(&p);
  ASSERT_EQ_INT(prog.fn_count, 0, "no functions");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * real rigg snippet
 * ---------------------------------------------------------------------- */

static void test_full_add_fn(void)
{
  Parser p;
  Program prog = parse("fn add(a: i32, b: i32) -> i32\n"
                       "{\n"
                       "    return a + b;\n"
                       "}\n",
                       &p);
  assert_ok(&p);
  ASSERT_EQ_INT(prog.fn_count, 1, "one fn");
  FnDecl *f = &prog.fns[0];
  ASSERT(memcmp(f->name, "add", 3) == 0, "name is add");
  ASSERT_EQ_INT(f->param_count, 2, "two params");
  ASSERT(f->return_type == TYPE_I32, "returns i32");
  ASSERT_EQ_INT(f->body.count, 1, "one stmt");
  Stmt *s = f->body.stmts[0];
  ASSERT(s->kind == STMT_RETURN, "is return");
  Expr *e = s->as.ret.value;
  ASSERT(e->kind == EXPR_BINARY, "return value is binary");
  ASSERT(e->as.binary.op == TOK_PLUS, "op is +");
  parser_free(&p);
}

/* -------------------------------------------------------------------------
 * entry point
 * ---------------------------------------------------------------------- */

int main(void)
{
  /* function declarations */
  run_test("fn_no_params_no_return", test_fn_no_params_no_return);
  run_test("fn_with_return_type", test_fn_with_return_type);
  run_test("fn_params", test_fn_params);
  run_test("fn_all_types_as_params", test_fn_all_types_as_params);
  run_test("multiple_fns", test_multiple_fns);

  /* let / const */
  run_test("let_immutable", test_let_immutable);
  run_test("let_mutable", test_let_mutable);
  run_test("const_decl", test_const_decl);

  /* return */
  run_test("return_value", test_return_value);
  run_test("return_bare", test_return_bare);

  /* literals */
  run_test("int_literal", test_int_literal);
  run_test("float_literal", test_float_literal);
  run_test("str_literal", test_str_literal);
  run_test("bool_literal_true", test_bool_literal_true);
  run_test("bool_literal_false", test_bool_literal_false);

  /* binary expressions */
  run_test("binary_add", test_binary_add);
  run_test("binary_precedence", test_binary_precedence);
  run_test("binary_left_associative", test_binary_left_associative);
  run_test("binary_comparison", test_binary_comparison);
  run_test("binary_logical", test_binary_logical);

  /* unary expressions */
  run_test("unary_bang", test_unary_bang);
  run_test("unary_minus", test_unary_minus);

  /* calls */
  run_test("call_no_args", test_call_no_args);
  run_test("call_with_args", test_call_with_args);
  run_test("qual_call", test_qual_call);
  run_test("qual_call_no_args", test_qual_call_no_args);

  /* assignment */
  run_test("assign", test_assign);
  run_test("assign_expr_rhs", test_assign_expr_rhs);
  run_test("update_expr", test_update_expr);
  run_test("ptr_index_read", test_ptr_index_read);
  run_test("ptr_index_assign", test_ptr_index_assign);
  run_test("cast_ptr_to_str", test_cast_ptr_to_str);
  run_test("cast_str_to_ptr", test_cast_str_to_ptr);
  run_test("cast_precedence", test_cast_precedence);

  /* grouping */
  run_test("grouped_expr", test_grouped_expr);

  /* control flow */
  run_test("if_no_else", test_if_no_else);
  run_test("if_with_else", test_if_with_else);
  run_test("if_else_if", test_if_else_if);
  run_test("while", test_while);
  run_test("for", test_for);
  run_test("loop", test_loop);
  run_test("continue", test_continue);

  /* line numbers */
  run_test("fn_line_number", test_fn_line_number);
  run_test("stmt_line_number", test_stmt_line_number);

  /* error recovery */
  run_test("error_on_top_level_non_fn", test_error_on_top_level_non_fn);
  run_test("error_missing_brace", test_error_missing_brace);
  run_test("error_recovers_second_fn", test_error_recovers_second_fn);
  run_test("empty_input", test_empty_input);

  /* integration */
  run_test("full_add_fn", test_full_add_fn);

  print_summary();
  return tc_suite_failed() > 0 ? 1 : 0;
}
