/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void *util_xmalloc(size_t n);
void *util_xrealloc(void *p, size_t n);
char *util_xstrdup(const char *s);
char *util_xsprintf(const char *fmt, ...);

#endif
