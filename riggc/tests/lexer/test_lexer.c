#include "lexer.h"
#include "test_common.h"

/* -------------------------------------------------------------------------
 * helpers
 * ---------------------------------------------------------------------- */

static Token next_tok(Lexer *l) { return lexer_next(l); }

static void assert_tok(Lexer *l, TokenKind kind, const char *text) {
  Token t = lexer_next(l);
  char msg[128];

  snprintf(msg, sizeof(msg), "expected kind %d got %d", kind, t.kind);
  ASSERT(t.kind == kind, msg);

  if (text) {
    snprintf(msg, sizeof(msg), "expected text '%s' got '%.*s'", text, t.len,
             t.start);
    ASSERT(t.len == (int)strlen(text) && memcmp(t.start, text, t.len) == 0,
           msg);
  }
}

static void assert_eof(Lexer *l) {
  Token t = lexer_next(l);
  ASSERT(t.kind == TOK_EOF, "expected EOF");
}

/* -------------------------------------------------------------------------
 * keywords
 * ---------------------------------------------------------------------- */

static void test_keywords(void) {
  const char *src =
      "fn let mut const return if else while loop break continue true false";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_FN, "fn");
  assert_tok(&l, TOK_LET, "let");
  assert_tok(&l, TOK_MUT, "mut");
  assert_tok(&l, TOK_CONST, "const");
  assert_tok(&l, TOK_RETURN, "return");
  assert_tok(&l, TOK_IF, "if");
  assert_tok(&l, TOK_ELSE, "else");
  assert_tok(&l, TOK_WHILE, "while");
  assert_tok(&l, TOK_LOOP, "loop");
  assert_tok(&l, TOK_BREAK, "break");
  assert_tok(&l, TOK_CONTINUE, "continue");
  assert_tok(&l, TOK_TRUE, "true");
  assert_tok(&l, TOK_FALSE, "false");
  assert_eof(&l);
}

static void test_types(void) {
  const char *src = "i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 bool str";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_I8, "i8");
  assert_tok(&l, TOK_I16, "i16");
  assert_tok(&l, TOK_I32, "i32");
  assert_tok(&l, TOK_I64, "i64");
  assert_tok(&l, TOK_U8, "u8");
  assert_tok(&l, TOK_U16, "u16");
  assert_tok(&l, TOK_U32, "u32");
  assert_tok(&l, TOK_U64, "u64");
  assert_tok(&l, TOK_F32, "f32");
  assert_tok(&l, TOK_F64, "f64");
  assert_tok(&l, TOK_BOOL, "bool");
  assert_tok(&l, TOK_STR, "str");
  assert_eof(&l);
}

/* -------------------------------------------------------------------------
 * identifiers
 * ---------------------------------------------------------------------- */

static void test_ident_simple(void) {
  const char *src = "foo bar baz";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_IDENT, "foo");
  assert_tok(&l, TOK_IDENT, "bar");
  assert_tok(&l, TOK_IDENT, "baz");
  assert_eof(&l);
}

static void test_ident_snake_case(void) {
  const char *src = "my_var another_one _private";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_IDENT, "my_var");
  assert_tok(&l, TOK_IDENT, "another_one");
  assert_tok(&l, TOK_IDENT, "_private");
  assert_eof(&l);
}

static void test_ident_not_keyword_prefix(void) {
  /* "fns" and "lets" look like keywords but are not */
  const char *src = "fns lets mutable";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_IDENT, "fns");
  assert_tok(&l, TOK_IDENT, "lets");
  assert_tok(&l, TOK_IDENT, "mutable");
  assert_eof(&l);
}

/* -------------------------------------------------------------------------
 * literals
 * ---------------------------------------------------------------------- */

static void test_int_literal(void) {
  const char *src = "0 42 1000";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_INT_LIT, "0");
  assert_tok(&l, TOK_INT_LIT, "42");
  assert_tok(&l, TOK_INT_LIT, "1000");
  assert_eof(&l);
}

static void test_float_literal(void) {
  const char *src = "3.14 0.5 100.0";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_FLOAT_LIT, "3.14");
  assert_tok(&l, TOK_FLOAT_LIT, "0.5");
  assert_tok(&l, TOK_FLOAT_LIT, "100.0");
  assert_eof(&l);
}

static void test_string_literal(void) {
  const char *src = "\"hello\" \"world\"";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_STR_LIT, "hello");
  assert_tok(&l, TOK_STR_LIT, "world");
  assert_eof(&l);
}

static void test_string_empty(void) {
  const char *src = "\"\"";
  Lexer l;
  lexer_init(&l, src);
  Token t = lexer_next(&l);
  ASSERT(t.kind == TOK_STR_LIT, "expected STR_LIT");
  ASSERT(t.len == 0, "expected empty string");
  assert_eof(&l);
}

/* -------------------------------------------------------------------------
 * punctuation and operators
 * ---------------------------------------------------------------------- */

static void test_punctuation(void) {
  const char *src = "( ) { } , ;";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_LPAREN, "(");
  assert_tok(&l, TOK_RPAREN, ")");
  assert_tok(&l, TOK_LBRACE, "{");
  assert_tok(&l, TOK_RBRACE, "}");
  assert_tok(&l, TOK_COMMA, ",");
  assert_tok(&l, TOK_SEMI, ";");
  assert_eof(&l);
}

static void test_arithmetic_ops(void) {
  const char *src = "+ - * / %";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_PLUS, "+");
  assert_tok(&l, TOK_MINUS, "-");
  assert_tok(&l, TOK_STAR, "*");
  assert_tok(&l, TOK_SLASH, "/");
  assert_tok(&l, TOK_PERCENT, "%");
  assert_eof(&l);
}

static void test_comparison_ops(void) {
  const char *src = "== != < > <= >=";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_EQ, "==");
  assert_tok(&l, TOK_NEQ, "!=");
  assert_tok(&l, TOK_LT, "<");
  assert_tok(&l, TOK_GT, ">");
  assert_tok(&l, TOK_LTE, "<=");
  assert_tok(&l, TOK_GTE, ">=");
  assert_eof(&l);
}

static void test_logical_ops(void) {
  const char *src = "&& || !";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_AND, "&&");
  assert_tok(&l, TOK_OR, "||");
  assert_tok(&l, TOK_BANG, "!");
  assert_eof(&l);
}

static void test_assign_vs_eq(void) {
  const char *src = "= ==";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_ASSIGN, "=");
  assert_tok(&l, TOK_EQ, "==");
  assert_eof(&l);
}

static void test_arrow(void) {
  const char *src = "->";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_ARROW, "->");
  assert_eof(&l);
}

static void test_minus_vs_arrow(void) {
  const char *src = "- ->";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_MINUS, "-");
  assert_tok(&l, TOK_ARROW, "->");
  assert_eof(&l);
}

static void test_colon_vs_colon_colon(void) {
  const char *src = ": ::";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_COLON, ":");
  assert_tok(&l, TOK_COLON_COLON, "::");
  assert_eof(&l);
}

/* -------------------------------------------------------------------------
 * whitespace and comments
 * ---------------------------------------------------------------------- */

static void test_whitespace_ignored(void) {
  const char *src = "   fn   \t  let  \n  mut  ";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_FN, "fn");
  assert_tok(&l, TOK_LET, "let");
  assert_tok(&l, TOK_MUT, "mut");
  assert_eof(&l);
}

static void test_line_comment(void) {
  const char *src = "fn // this is ignored\nlet";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_FN, "fn");
  assert_tok(&l, TOK_LET, "let");
  assert_eof(&l);
}

static void test_block_comment(void) {
  const char *src = "fn /* this is\nignored */ let";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_FN, "fn");
  assert_tok(&l, TOK_LET, "let");
  assert_eof(&l);
}

static void test_comment_only(void) {
  const char *src = "// nothing here";
  Lexer l;
  lexer_init(&l, src);
  assert_eof(&l);
}

/* -------------------------------------------------------------------------
 * line numbers
 * ---------------------------------------------------------------------- */

static void test_line_numbers(void) {
  const char *src = "fn\nlet\n\nmut";
  Lexer l;
  lexer_init(&l, src);
  Token t;

  t = lexer_next(&l);
  ASSERT(t.line == 1, "fn should be line 1");
  t = lexer_next(&l);
  ASSERT(t.line == 2, "let should be line 2");
  t = lexer_next(&l);
  ASSERT(t.line == 4, "mut should be line 4");
  assert_eof(&l);
}

static void test_line_numbers_block_comment(void) {
  const char *src = "fn /* line1\nline2\nline3 */ let";
  Lexer l;
  lexer_init(&l, src);
  Token t;

  t = lexer_next(&l);
  ASSERT(t.line == 1, "fn should be line 1");
  t = lexer_next(&l);
  ASSERT(t.line == 3, "let should be line 3");
  assert_eof(&l);
}

/* -------------------------------------------------------------------------
 * real rigg snippets
 * ---------------------------------------------------------------------- */

static void test_fn_declaration(void) {
  const char *src = "fn add(a: i32, b: i32) -> i32";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_FN, "fn");
  assert_tok(&l, TOK_IDENT, "add");
  assert_tok(&l, TOK_LPAREN, "(");
  assert_tok(&l, TOK_IDENT, "a");
  assert_tok(&l, TOK_COLON, ":");
  assert_tok(&l, TOK_I32, "i32");
  assert_tok(&l, TOK_COMMA, ",");
  assert_tok(&l, TOK_IDENT, "b");
  assert_tok(&l, TOK_COLON, ":");
  assert_tok(&l, TOK_I32, "i32");
  assert_tok(&l, TOK_RPAREN, ")");
  assert_tok(&l, TOK_ARROW, "->");
  assert_tok(&l, TOK_I32, "i32");
  assert_eof(&l);
}

static void test_let_declaration(void) {
  const char *src = "let mut count: i32 = 0;";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_LET, "let");
  assert_tok(&l, TOK_MUT, "mut");
  assert_tok(&l, TOK_IDENT, "count");
  assert_tok(&l, TOK_COLON, ":");
  assert_tok(&l, TOK_I32, "i32");
  assert_tok(&l, TOK_ASSIGN, "=");
  assert_tok(&l, TOK_INT_LIT, "0");
  assert_tok(&l, TOK_SEMI, ";");
  assert_eof(&l);
}

static void test_cross_concept_call(void) {
  const char *src = "buffer::write(pos, ch);";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_IDENT, "buffer");
  assert_tok(&l, TOK_COLON_COLON, "::");
  assert_tok(&l, TOK_IDENT, "write");
  assert_tok(&l, TOK_LPAREN, "(");
  assert_tok(&l, TOK_IDENT, "pos");
  assert_tok(&l, TOK_COMMA, ",");
  assert_tok(&l, TOK_IDENT, "ch");
  assert_tok(&l, TOK_RPAREN, ")");
  assert_tok(&l, TOK_SEMI, ";");
  assert_eof(&l);
}

static void test_if_else(void) {
  const char *src = "if x < 10 { return x; } else { return 10; }";
  Lexer l;
  lexer_init(&l, src);
  assert_tok(&l, TOK_IF, "if");
  assert_tok(&l, TOK_IDENT, "x");
  assert_tok(&l, TOK_LT, "<");
  assert_tok(&l, TOK_INT_LIT, "10");
  assert_tok(&l, TOK_LBRACE, "{");
  assert_tok(&l, TOK_RETURN, "return");
  assert_tok(&l, TOK_IDENT, "x");
  assert_tok(&l, TOK_SEMI, ";");
  assert_tok(&l, TOK_RBRACE, "}");
  assert_tok(&l, TOK_ELSE, "else");
  assert_tok(&l, TOK_LBRACE, "{");
  assert_tok(&l, TOK_RETURN, "return");
  assert_tok(&l, TOK_INT_LIT, "10");
  assert_tok(&l, TOK_SEMI, ";");
  assert_tok(&l, TOK_RBRACE, "}");
  assert_eof(&l);
}

static void test_empty_input(void) {
  const char *src = "";
  Lexer l;
  lexer_init(&l, src);
  assert_eof(&l);
  /* calling again after EOF should keep returning EOF */
  assert_eof(&l);
}

/* -------------------------------------------------------------------------
 * entry point
 * ---------------------------------------------------------------------- */

int main(void) {
  run_test("keywords", test_keywords);
  run_test("types", test_types);
  run_test("ident_simple", test_ident_simple);
  run_test("ident_snake_case", test_ident_snake_case);
  run_test("ident_not_keyword_prefix", test_ident_not_keyword_prefix);
  run_test("int_literal", test_int_literal);
  run_test("float_literal", test_float_literal);
  run_test("string_literal", test_string_literal);
  run_test("string_empty", test_string_empty);
  run_test("punctuation", test_punctuation);
  run_test("arithmetic_ops", test_arithmetic_ops);
  run_test("comparison_ops", test_comparison_ops);
  run_test("logical_ops", test_logical_ops);
  run_test("assign_vs_eq", test_assign_vs_eq);
  run_test("arrow", test_arrow);
  run_test("minus_vs_arrow", test_minus_vs_arrow);
  run_test("colon_vs_colon_colon", test_colon_vs_colon_colon);
  run_test("whitespace_ignored", test_whitespace_ignored);
  run_test("line_comment", test_line_comment);
  run_test("block_comment", test_block_comment);
  run_test("comment_only", test_comment_only);
  run_test("line_numbers", test_line_numbers);
  run_test("line_numbers_block_comment", test_line_numbers_block_comment);
  run_test("fn_declaration", test_fn_declaration);
  run_test("let_declaration", test_let_declaration);
  run_test("cross_concept_call", test_cross_concept_call);
  run_test("if_else", test_if_else);
  run_test("empty_input", test_empty_input);

  print_summary();
  return tc_suite_failed() > 0 ? 1 : 0;
}
