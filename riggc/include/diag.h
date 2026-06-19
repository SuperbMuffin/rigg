/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef DIAG_H
#define DIAG_H

#include <stdio.h>

/* True when ANSI colors should be emitted on stderr. */
int diag_use_color(void);

void diag_print_error(FILE *out, const char *code, const char *message);
void diag_print_location(FILE *out, const char *file, int line);
void diag_print_context(FILE *out, const char *text);
void diag_print_cycle(FILE *out, const char **names, int count);

#endif
