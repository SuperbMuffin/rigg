#include "lexer.h"
#include <ctype.h>
#include <string.h>

void lexer_init(Lexer *l, const char *src)
{
  l->src = src;
  l->pos = 0;
  l->line = 1;
}

static char peek(const Lexer *l)
{
  return l->src[l->pos];
}

static char peek2(const Lexer *l)
{
  if (l->src[l->pos] == '\0' || l->src[l->pos + 1] == '\0')
    return '\0';
  return l->src[l->pos + 1];
}

static char advance(Lexer *l)
{
  char c = l->src[l->pos++];
  if (c == '\n')
    l->line++;
  return c;
}

static void skip_whitespace_and_comments(Lexer *l)
{
  while (1)
  {
    char c = peek(l);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
    {
      advance(l);
      continue;
    }
    if (c == '/' && peek2(l) == '/')
    {
      while (peek(l) != '\n' && peek(l) != '\0')
        advance(l);
      continue;
    }
    if (c == '/' && peek2(l) == '*')
    {
      advance(l);
      advance(l);
      while (!(peek(l) == '*' && peek2(l) == '/') && peek(l) != '\0')
        advance(l);
      if (peek(l) != '\0')
      {
        advance(l);
        advance(l);
      }
      continue;
    }
    break;
  }
}

static TokenKind keyword_or_ident(const char *start, int len)
{
#define KW(s, tok)                                          \
  if (len == (int) strlen(s) && memcmp(start, s, len) == 0) \
    return tok;
  KW("fn", TOK_FN)
  KW("let", TOK_LET)
  KW("mut", TOK_MUT)
  KW("const", TOK_CONST)
  KW("return", TOK_RETURN)
  KW("if", TOK_IF)
  KW("else", TOK_ELSE)
  KW("while", TOK_WHILE)
  KW("loop", TOK_LOOP)
  KW("break", TOK_BREAK)
  KW("continue", TOK_CONTINUE)
  KW("extern", TOK_EXTERN)
  KW("true", TOK_TRUE)
  KW("false", TOK_FALSE)
  KW("i8", TOK_I8)
  KW("i16", TOK_I16)
  KW("i32", TOK_I32)
  KW("i64", TOK_I64)
  KW("u8", TOK_U8)
  KW("u16", TOK_U16)
  KW("u32", TOK_U32)
  KW("u64", TOK_U64)
  KW("f32", TOK_F32)
  KW("f64", TOK_F64)
  KW("bool", TOK_BOOL)
  KW("str", TOK_STR)
#undef KW
  return TOK_IDENT;
}

Token lexer_next(Lexer *l)
{
  skip_whitespace_and_comments(l);

  Token t;
  t.line = l->line;
  t.start = &l->src[l->pos];
  t.len = 0;

  char c = peek(l);
  if (c == '\0')
  {
    t.kind = TOK_EOF;
    return t;
  }

  if (isalpha(c) || c == '_')
  {
    while (isalnum(peek(l)) || peek(l) == '_')
      advance(l);
    t.len = &l->src[l->pos] - t.start;
    t.kind = keyword_or_ident(t.start, t.len);
    return t;
  }

  if (isdigit(c))
  {
    while (isdigit(peek(l)))
      advance(l);
    t.kind = TOK_INT_LIT;
    if (peek(l) == '.' && isdigit(peek2(l)))
    {
      advance(l);
      while (isdigit(peek(l)))
        advance(l);
      t.kind = TOK_FLOAT_LIT;
    }
    t.len = &l->src[l->pos] - t.start;
    return t;
  }

  /* Escape sequences not yet supported; first unescaped '"' ends the literal */
  if (c == '"')
  {
    advance(l);
    t.start = &l->src[l->pos];
    while (peek(l) != '"' && peek(l) != '\0')
      advance(l);
    t.len = &l->src[l->pos] - t.start;
    t.kind = TOK_STR_LIT;
    if (peek(l) == '"')
      advance(l);
    return t;
  }

  advance(l);
  switch (c)
  {
    case '(':
      t.kind = TOK_LPAREN;
      break;
    case ')':
      t.kind = TOK_RPAREN;
      break;
    case '{':
      t.kind = TOK_LBRACE;
      break;
    case '}':
      t.kind = TOK_RBRACE;
      break;
    case ',':
      t.kind = TOK_COMMA;
      break;
    case ';':
      t.kind = TOK_SEMI;
      break;
    case '+':
      t.kind = TOK_PLUS;
      break;
    case '*':
      t.kind = TOK_STAR;
      break;
    case '/':
      t.kind = TOK_SLASH;
      break;
    case '%':
      t.kind = TOK_PERCENT;
      break;
    case ':':
      if (peek(l) == ':')
      {
        advance(l);
        t.kind = TOK_COLON_COLON;
      }
      else
        t.kind = TOK_COLON;
      break;
    case '-':
      if (peek(l) == '>')
      {
        advance(l);
        t.kind = TOK_ARROW;
      }
      else
        t.kind = TOK_MINUS;
      break;
    case '=':
      if (peek(l) == '=')
      {
        advance(l);
        t.kind = TOK_EQ;
      }
      else
        t.kind = TOK_ASSIGN;
      break;
    case '!':
      if (peek(l) == '=')
      {
        advance(l);
        t.kind = TOK_NEQ;
      }
      else
        t.kind = TOK_BANG;
      break;
    case '<':
      if (peek(l) == '=')
      {
        advance(l);
        t.kind = TOK_LTE;
      }
      else
        t.kind = TOK_LT;
      break;
    case '>':
      if (peek(l) == '=')
      {
        advance(l);
        t.kind = TOK_GTE;
      }
      else
        t.kind = TOK_GT;
      break;
    case '&':
      if (peek(l) == '&')
      {
        advance(l);
        t.kind = TOK_AND;
      }
      else
        t.kind = TOK_ERROR;
      break;
    case '|':
      if (peek(l) == '|')
      {
        advance(l);
        t.kind = TOK_OR;
      }
      else
        t.kind = TOK_ERROR;
      break;
    case '.':
      if (peek(l) == '.' && peek2(l) == '.')
      {
        advance(l);
        advance(l);
        t.kind = TOK_ELLIPSIS;
      }
      else
        t.kind = TOK_ERROR;
      break;
  }

  t.len = &l->src[l->pos] - t.start;
  return t;
}
