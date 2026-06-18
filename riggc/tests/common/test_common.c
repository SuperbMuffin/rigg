#include "test_common.h"

int tests_run = 0;
int tests_failed = 0;

/* suite-level counters (live in the parent process) */
static int suite_passed = 0;
static int suite_failed = 0;

void tc_fail(const char *file, int line, const char *msg)
{
  fprintf(stderr, "FAIL [%s:%d] %s\n", file, line, msg);
  tests_failed++;
}

static int in_child = 0;

void run_test(const char *name, void (*fn)(void))
{
  /* if we're already inside a forked test child, just run directly */
  if (in_child)
  {
    fn();
    return;
  }

  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  if (pid == 0)
  {
    in_child = 1;
    fn();
    exit(tests_failed > 0 ? 1 : 0);
  }

  /* parent: wait and interpret result */
  int status;
  waitpid(pid, &status, 0);

  if (WIFSIGNALED(status))
  {
    printf("CRASH [%s] (signal %d)\n", name, WTERMSIG(status));
    suite_failed++;
  }
  else if (WEXITSTATUS(status) != 0)
  {
    printf("FAIL  [%s]\n", name);
    suite_failed++;
  }
  else
  {
    printf("PASS  [%s]\n", name);
    suite_passed++;
  }
}

void print_summary(void)
{
  int total = suite_passed + suite_failed;
  printf("\n%d/%d tests passed\n", suite_passed, total);
}

int tc_suite_failed(void)
{
  return suite_failed;
}
