/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef TOKEN_H
#define TOKEN_H

typedef enum
{
  TOK_FN,
  TOK_LET,
  TOK_MUT,
  TOK_CONST,
  TOK_RETURN,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_LOOP,
  TOK_BREAK,
  TOK_CONTINUE,
  TOK_EXTERN,
  TOK_VAR,
  TOK_TRUE,
  TOK_FALSE,

  TOK_I8,
  TOK_I16,
  TOK_I32,
  TOK_I64,
  TOK_U8,
  TOK_U16,
  TOK_U32,
  TOK_U64,
  TOK_F32,
  TOK_F64,
  TOK_BOOL,
  TOK_STR,
  TOK_PTR,

  TOK_INT_LIT,
  TOK_FLOAT_LIT,
  TOK_STR_LIT, /* start/len span content only, excluding surrounding quotes */
  TOK_IDENT,

  TOK_LPAREN,      // (
  TOK_RPAREN,      // )
  TOK_LBRACKET,    // [
  TOK_RBRACKET,    // ]
  TOK_LBRACE,      // {
  TOK_RBRACE,      // }
  TOK_COMMA,       // ,
  TOK_SEMI,        // ;
  TOK_COLON,       // :
  TOK_COLON_COLON, // ::

  TOK_PLUS,     // +
  TOK_MINUS,    // -
  TOK_STAR,     // *
  TOK_SLASH,    // /
  TOK_PERCENT,  // %
  TOK_EQ,       // ==
  TOK_NEQ,      // !=
  TOK_LT,       // <
  TOK_GT,       // >
  TOK_LTE,      // <=
  TOK_GTE,      // >=
  TOK_AND,      // &&
  TOK_OR,       // ||
  TOK_BANG,     // !
  TOK_ASSIGN,   // =
  TOK_ARROW,    // ->
  TOK_ELLIPSIS, // ...
  TOK_AS,       // as

  TOK_EOF,
  TOK_ERROR,
} TokenKind;

typedef struct
{
  TokenKind kind;
  const char *start;
  int len;
  int line;
} Token;

#endif
