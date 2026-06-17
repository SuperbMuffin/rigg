#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * per-child state (meaningful only inside a forked test process)
 * ---------------------------------------------------------------------- */

extern int tests_run;
extern int tests_failed;

void tc_fail(const char *file, int line, const char *msg);

/* -------------------------------------------------------------------------
 * assert macros
 * ---------------------------------------------------------------------- */

#define ASSERT(cond, msg)                                                      \
  do {                                                                         \
    tests_run++;                                                               \
    if (!(cond))                                                               \
      tc_fail(__FILE__, __LINE__, msg);                                        \
  } while (0)

#define ASSERT_EQ_INT(a, b, msg)                                               \
  do {                                                                         \
    tests_run++;                                                               \
    int _a = (a), _b = (b);                                                    \
    if (_a != _b) {                                                            \
      fprintf(stderr, "  expected %d, got %d\n", _b, _a);                      \
      tc_fail(__FILE__, __LINE__, msg);                                        \
    }                                                                          \
  } while (0)

#define ASSERT_EQ_CHAR(a, b, msg)                                              \
  do {                                                                         \
    tests_run++;                                                               \
    char _a = (a), _b = (b);                                                   \
    if (_a != _b) {                                                            \
      fprintf(stderr,                                                          \
              "  expected '%c' (0x%02x),"                                      \
              " got '%c' (0x%02x)\n",                                          \
              (unsigned char)_b, (unsigned char)_b, (unsigned char)_a,         \
              (unsigned char)_a);                                              \
      tc_fail(__FILE__, __LINE__, msg);                                        \
    }                                                                          \
  } while (0)

#define ASSERT_EQ_STR(a, b, msg)                                               \
  do {                                                                         \
    tests_run++;                                                               \
    const char *_a = (a), *_b = (b);                                           \
    if (strcmp(_a, _b) != 0) {                                                 \
      fprintf(stderr, "  expected \"%s\", got \"%s\"\n", _b, _a);              \
      tc_fail(__FILE__, __LINE__, msg);                                        \
    }                                                                          \
  } while (0)

/* -------------------------------------------------------------------------
 * fork-based test runner
 * ---------------------------------------------------------------------- */

void run_test(const char *name, void (*fn)(void));
void print_summary(void);
int tc_suite_failed(void);

#endif /* TEST_COMMON_H */
