#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *util_xmalloc(size_t n)
{
  void *p = malloc(n);
  if (!p)
  {
    fprintf(stderr, "rigg: out of memory\n");
    exit(1);
  }
  return p;
}

void *util_xrealloc(void *p, size_t n)
{
  p = realloc(p, n);
  if (!p)
  {
    fprintf(stderr, "rigg: out of memory\n");
    exit(1);
  }
  return p;
}

char *util_xstrdup(const char *s)
{
  if (!s)
    return NULL;
  size_t n = strlen(s) + 1;
  char *p = util_xmalloc(n);
  memcpy(p, s, n);
  return p;
}

char *util_xsprintf(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  char *buf = util_xmalloc((size_t) n + 1);
  va_start(ap, fmt);
  vsnprintf(buf, (size_t) n + 1, fmt, ap);
  va_end(ap);
  return buf;
}
