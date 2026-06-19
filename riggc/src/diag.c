/*
 * SPDX-License-Identifier: MPL-2.0
 */

#define _POSIX_C_SOURCE 200809L

#include "diag.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define C_RED "\033[1;31m"
#define C_CYAN "\033[1;36m"
#define C_ARROW "\033[0;36m"
#define C_BOLD "\033[1m"
#define C_DIM "\033[2m"
#define C_RESET "\033[0m"

int diag_use_color(void)
{
  static int cached = -1;
  if (cached >= 0)
    return cached;

  const char *force = getenv("FORCE_COLOR");
  if (force && force[0] != '\0' && strcmp(force, "0") != 0)
  {
    cached = 1;
    return cached;
  }

  const char *no_color = getenv("NO_COLOR");
  if (no_color && no_color[0] != '\0')
  {
    cached = 0;
    return cached;
  }

  cached = isatty(fileno(stderr));
  return cached;
}

void diag_print_error(FILE *out, const char *code, const char *message)
{
  if (diag_use_color())
    fprintf(out, C_RED "Error" C_RESET " " C_CYAN "%s" C_RESET ": " C_BOLD "%s" C_RESET "\n", code,
            message);
  else
    fprintf(out, "Error %s: %s\n", code, message);
}

void diag_print_location(FILE *out, const char *file, int line)
{
  if (!file)
    return;

  if (diag_use_color())
  {
    if (line)
      fprintf(out, "\n" C_ARROW "--> " C_BOLD "%s" C_RESET C_CYAN ":%d" C_RESET "\n", file, line);
    else
      fprintf(out, "\n" C_ARROW "--> " C_BOLD "%s" C_RESET "\n", file);
  }
  else if (line)
    fprintf(out, "\n  --> %s:%d\n", file, line);
  else
    fprintf(out, "\n  --> %s\n", file);
}

void diag_print_context(FILE *out, const char *text)
{
  if (!text)
    return;

  if (diag_use_color())
    fprintf(out, "\n" C_DIM "  %s" C_RESET "\n", text);
  else
    fprintf(out, "\n  %s\n", text);
}

void diag_print_cycle(FILE *out, const char **names, int count)
{
  if (count <= 0)
    return;

  fputc('\n', out);
  for (int i = 0; i < count; i++)
  {
    if (i)
    {
      if (diag_use_color())
        fprintf(out, C_ARROW " -> " C_RESET);
      else
        fputs(" -> ", out);
    }
    else
      fputs("  ", out);

    if (diag_use_color())
      fprintf(out, C_BOLD "%s" C_RESET, names[i]);
    else
      fputs(names[i], out);
  }
  fputc('\n', out);
}
