#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct
{
  const char *src;
  int pos;
  int line;
} Lexer;

void lexer_init(Lexer *l, const char *src);
Token lexer_next(Lexer *l);

#endif
