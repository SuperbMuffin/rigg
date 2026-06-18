/* Rigg cast runtime — linked automatically by riggc. Do not call directly. */

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

[[noreturn]] static void rigg_runtime_error(const char *msg)
{
  fprintf(stderr, "rigg: runtime error: %s\n", msg);
  abort();
}

static void check_parse_end(const char *s, char *end)
{
  if (*s == '\0' || *end != '\0')
    rigg_runtime_error("invalid integer string");
}

static long long parse_decimal_ll(const char *s)
{
  char *end = NULL;
  errno = 0;
  long long val = strtoll(s, &end, 10);
  check_parse_end(s, end);
  if (errno == ERANGE)
    rigg_runtime_error("integer overflow");
  return val;
}

int8_t rigg_str_to_i8(const char *s)
{
  long long val = parse_decimal_ll(s);
  if (val < INT8_MIN || val > INT8_MAX)
    rigg_runtime_error("integer overflow");
  return (int8_t) val;
}

int16_t rigg_str_to_i16(const char *s)
{
  long long val = parse_decimal_ll(s);
  if (val < INT16_MIN || val > INT16_MAX)
    rigg_runtime_error("integer overflow");
  return (int16_t) val;
}

int32_t rigg_str_to_i32(const char *s)
{
  long long val = parse_decimal_ll(s);
  if (val < INT32_MIN || val > INT32_MAX)
    rigg_runtime_error("integer overflow");
  return (int32_t) val;
}

int64_t rigg_str_to_i64(const char *s)
{
  long long val = parse_decimal_ll(s);
  return (int64_t) val;
}

static char *rigg_strdup(const char *s)
{
  size_t n = strlen(s) + 1;
  char *buf = malloc(n);
  if (!buf)
    rigg_runtime_error("out of memory");
  memcpy(buf, s, n);
  return buf;
}

char *rigg_i8_to_str(int8_t n)
{
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", (int) n);
  return rigg_strdup(buf);
}

char *rigg_i16_to_str(int16_t n)
{
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", (int) n);
  return rigg_strdup(buf);
}

char *rigg_i32_to_str(int32_t n)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", (int) n);
  return rigg_strdup(buf);
}

char *rigg_i64_to_str(int64_t n)
{
  char buf[24];
  snprintf(buf, sizeof(buf), "%lld", (long long) n);
  return rigg_strdup(buf);
}

char *rigg_bool_to_str(int b)
{
  return rigg_strdup(b ? "true" : "false");
}

int rigg_str_to_bool(const char *s)
{
  if (strcmp(s, "true") == 0 || strcmp(s, "1") == 0)
    return 1;
  if (strcmp(s, "false") == 0 || strcmp(s, "0") == 0)
    return 0;
  rigg_runtime_error("invalid bool string");
}

int rigg_str_eq(const char *a, const char *b)
{
  return strcmp(a, b) == 0;
}

int rigg_str_ne(const char *a, const char *b)
{
  return strcmp(a, b) != 0;
}
