#include "project.h"
#include "sema.h"
#include "test_common.h"
#include <unistd.h>

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/* Construct a path relative to the fixtures directory.
   FIXTURES_DIR is injected by the build as -DFIXTURES_DIR="..." */
#ifndef FIXTURES_DIR
#error "FIXTURES_DIR must be defined at compile time (e.g. -DFIXTURES_DIR=...)"
#endif

static const char *fix(const char *name)
{
  static char buf[1024];
  snprintf(buf, sizeof(buf), "%s/%s", FIXTURES_DIR, name);
  return buf;
}

/* Load a fixture project and run sema. Fills *proj and *res.
   Returns 0 if project_load succeeded, -1 on fatal I/O error. */
static int load(const char *fixture, Project *proj, SemaResult *res)
{
  int rc = project_load(proj, fix(fixture));
  if (rc == 0)
    sema_check(proj, res);
  return rc;
}

static void teardown(Project *proj, SemaResult *res)
{
  sema_free(res);
  project_free(proj);
}

/* Assert that the result has zero errors. Prints all errors on failure. */
static void assert_ok(const SemaResult *res)
{
  if (res->count > 0)
  {
    sema_print(res);
    ASSERT(res->count == 0, "expected no sema errors");
  }
}

/* Assert that exactly one error with the given code exists. */
static void assert_error(const SemaResult *res, const char *code)
{
  int found = 0;
  for (int i = 0; i < res->count; i++)
  {
    if (strcmp(res->errors[i].code, code) == 0)
      found++;
  }
  if (found != 1)
  {
    sema_print(res);
    char msg[64];
    snprintf(msg, sizeof(msg), "expected exactly 1 %s error, got %d", code, found);
    ASSERT(found == 1, msg);
  }
}

/* Assert that at least one error with the given code exists. */
static void assert_has_error(const SemaResult *res, const char *code)
{
  for (int i = 0; i < res->count; i++)
  {
    if (strcmp(res->errors[i].code, code) == 0)
      return;
  }
  sema_print(res);
  char msg[64];
  snprintf(msg, sizeof(msg), "expected at least one %s error, got none", code);
  ASSERT(0, msg);
}

/* Assert that no error with the given code exists. */
static void assert_no_error(const SemaResult *res, const char *code)
{
  for (int i = 0; i < res->count; i++)
  {
    if (strcmp(res->errors[i].code, code) == 0)
    {
      sema_print(res);
      char msg[64];
      snprintf(msg, sizeof(msg), "expected no %s error but one was emitted", code);
      ASSERT(0, msg);
      return;
    }
  }
}

/* -------------------------------------------------------------------------
 * ok — projects that should pass cleanly
 * ---------------------------------------------------------------------- */

static void test_ok_minimal(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("ok_minimal", &proj, &res) == 0, "project_load succeeded");
  assert_ok(&res);
  teardown(&proj, &res);
}

static void test_ok_main_return_type(void)
{
  /* main() -> i32 is legal — E002 must NOT fire for a return type */
  Project proj;
  SemaResult res;
  ASSERT(load("ok_main_return_type", &proj, &res) == 0, "project_load succeeded");
  assert_no_error(&res, "E002");
  assert_ok(&res);
  teardown(&proj, &res);
}

static void test_ok_multi_concept(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("ok_multi_concept", &proj, &res) == 0, "project_load succeeded");
  assert_ok(&res);
  teardown(&proj, &res);
}

static void test_ok_impl_file(void)
{
  /* .impl file alongside .fn files — should not cause any errors */
  Project proj;
  SemaResult res;
  ASSERT(load("ok_impl_file", &proj, &res) == 0, "project_load succeeded");
  assert_ok(&res);
  teardown(&proj, &res);
}

static void test_ok_helper_fns(void)
{
  /* private helper above primary fn in a .fn file — should be fine */
  Project proj;
  SemaResult res;
  ASSERT(load("ok_helper_fns", &proj, &res) == 0, "project_load succeeded");
  assert_ok(&res);
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * E001 — missing entry point
 * ---------------------------------------------------------------------- */

static void test_e001_missing_main(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("e001_missing_main", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "E001");
  teardown(&proj, &res);
}

static void test_e001_declared_main_missing_file(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("e001_declared_main_missing_file", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "E001");
  teardown(&proj, &res);
}

static void test_e001_main_fn_missing_main_function(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("e001_main_fn_missing_main_function", &proj, &res) == 0, "project_load succeeded");
  assert_has_error(&res, "F001");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * E002 — invalid main signature (params only; return type is now allowed)
 * ---------------------------------------------------------------------- */

static void test_e002_main_has_params(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("e002_main_has_params", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "E002");
  teardown(&proj, &res);
}

static void test_e002_main_bad_return_type(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("e002_main_bad_return_type", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "E002");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * F001 — missing primary function
 * ---------------------------------------------------------------------- */

static void test_f001_missing_primary(void)
{
  /* math/add.fn exists but contains no fn add() */
  Project proj;
  SemaResult res;
  ASSERT(load("f001_missing_primary", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "F001");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * F003 — function name mismatch
 * ---------------------------------------------------------------------- */

static void test_f003_name_mismatch(void)
{
  /* math/add.fn declares fn subtract() instead of fn add() */
  Project proj;
  SemaResult res;
  ASSERT(load("f003_name_mismatch", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "F003");
  teardown(&proj, &res);
}

static void test_f002_duplicate_primary(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("f002_duplicate_primary", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "F002");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * C001 — concept directory missing
 * ---------------------------------------------------------------------- */

static void test_c001_missing_dir(void)
{
  /* project.meta declares 'ghost' but no ghost/ directory exists */
  Project proj;
  SemaResult res;
  ASSERT(load("c001_missing_dir", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "C001");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * C002 — undeclared concept directory
 * ---------------------------------------------------------------------- */

static void test_c002_undeclared_dir(void)
{
  /* orphan/ has a .fn file but is not declared in project.meta */
  Project proj;
  SemaResult res;
  ASSERT(load("c002_undeclared_dir", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "C002");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * I001 — illegal cross-concept call (edge not in graph)
 * ---------------------------------------------------------------------- */

static void test_i001_no_edge(void)
{
  /* buffer calls core::alloc() but no edge buffer -> core in project.meta */
  Project proj;
  SemaResult res;
  ASSERT(load("i001_no_edge", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "I001");
  teardown(&proj, &res);
}

static void test_i001_not_emitted_for_main(void)
{
  /* main declares -> buffer in project.meta, so main::buffer calls are legal.
     The only I001 should be buffer->core (undeclared edge), not main->buffer.
   */
  Project proj;
  SemaResult res;
  ASSERT(load("i001_no_edge", &proj, &res) == 0, "project_load succeeded");
  int count = 0;
  for (int i = 0; i < res.count; i++)
    if (strcmp(res.errors[i].code, "I001") == 0)
      count++;
  ASSERT(count == 1, "exactly one I001 (buffer->core), not main->buffer");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * I002 — unknown concept in :: call
 * ---------------------------------------------------------------------- */

static void test_i002_unknown_concept(void)
{
  /* main calls ghost::vanish() — 'ghost' is not in project.meta */
  Project proj;
  SemaResult res;
  ASSERT(load("i002_unknown_concept", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "I002");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * I003 — unknown function in :: call
 * ---------------------------------------------------------------------- */

static void test_i003_unknown_fn(void)
{
  /* main calls math::subtract() but math only exports add() */
  Project proj;
  SemaResult res;
  ASSERT(load("i003_unknown_fn", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "I003");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * I004 — accessing internal implementation function
 * ---------------------------------------------------------------------- */

static void test_i004_impl_access(void)
{
  /* main calls math::clamp() which is defined in math/helpers.impl */
  Project proj;
  SemaResult res;
  ASSERT(load("i004_impl_access", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "I004");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * S — parse errors surfaced through sema
 * ---------------------------------------------------------------------- */

static void test_s001_parse_error_surfaced(void)
{
  /* math/add.fn has a syntax error — sema must surface it as S001 */
  Project proj;
  SemaResult res;
  ASSERT(load("s001_parse_error", &proj, &res) == 0, "project_load succeeded");
  assert_has_error(&res, "S001");
  teardown(&proj, &res);
}

static void assert_no_duplicate_errors(const SemaResult *res)
{
  for (int i = 0; i < res->count; i++)
  {
    for (int j = i + 1; j < res->count; j++)
    {
      const SemaError *a = &res->errors[i];
      const SemaError *b = &res->errors[j];
      int same_file = (a->file == NULL && b->file == NULL) ||
                      (a->file && b->file && strcmp(a->file, b->file) == 0);
      if (strcmp(a->code, b->code) == 0 && a->line == b->line && same_file &&
          strcmp(a->message, b->message) == 0)
      {
        char msg[256];
        snprintf(msg, sizeof(msg), "duplicate error %s at %s:%d", a->code,
                 a->file ? a->file : "<no file>", a->line);
        ASSERT(0, msg);
      }
    }
  }
}

static void test_parse_errors_not_doubled(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("s001_parse_error", &proj, &res) == 0, "project_load succeeded");
  assert_no_duplicate_errors(&res);
  teardown(&proj, &res);
}

static void test_s001_main_parse_error_not_doubled(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("s001_main_parse_error", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "S001");
  assert_no_duplicate_errors(&res);
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * T001 — type mismatch
 * ---------------------------------------------------------------------- */

static void test_t001_ptr_index_non_ptr(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t001_ptr_index_non_ptr", &proj, &res) == 0, "project_load succeeded");
  assert_has_error(&res, "T001");
  teardown(&proj, &res);
}

static void test_t001_type_mismatch(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t001_type_mismatch", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "T001");
  teardown(&proj, &res);
}

static void test_ok_ptr_str_cast(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("ok_ptr_str_cast", &proj, &res) == 0, "project_load succeeded");
  assert_ok(&res);
  teardown(&proj, &res);
}

static void test_ok_cast_conversions(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("ok_cast_conversions", &proj, &res) == 0, "project_load succeeded");
  assert_ok(&res);
  teardown(&proj, &res);
}

static void test_t001_invalid_cast(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t001_invalid_cast", &proj, &res) == 0, "project_load succeeded");
  assert_has_error(&res, "T001");
  teardown(&proj, &res);
}

static void test_t001_implicit_ptr_str_return(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t001_implicit_ptr_str_return", &proj, &res) == 0, "project_load succeeded");
  assert_has_error(&res, "T001");
  teardown(&proj, &res);
}

static void test_t001_implicit_ptr_str_let(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t001_implicit_ptr_str_let", &proj, &res) == 0, "project_load succeeded");
  assert_has_error(&res, "T001");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * T003 — missing return
 * ---------------------------------------------------------------------- */

static void test_t003_missing_return(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t003_missing_return", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "T003");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * T004 — return value in void function
 * ---------------------------------------------------------------------- */

static void test_t004_return_in_void(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t004_return_in_void", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "T004");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * T005 — reassignment of immutable variable
 * ---------------------------------------------------------------------- */

static void test_t005_immutable_reassign(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t005_immutable_reassign", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "T005");
  teardown(&proj, &res);
}

static void test_t005_for_post_increment_immutable(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t005_for_post_increment_immutable", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "T005");
  teardown(&proj, &res);
}

static void test_t001_for_post_increment_type(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t001_for_post_increment_type", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "T001");
  teardown(&proj, &res);
}

static void test_ok_for_loop(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("ok_for_loop", &proj, &res) == 0, "project_load succeeded");
  if (res.count > 0)
  {
    sema_print(&res);
    ASSERT(res.count == 0, "no spurious errors in for loop fixture");
  }
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * error count / no unexpected errors
 * ---------------------------------------------------------------------- */

static void test_ok_no_spurious_errors(void)
{
  /* A clean multi-concept project must produce zero errors of any kind */
  Project proj;
  SemaResult res;
  ASSERT(load("ok_multi_concept", &proj, &res) == 0, "project_load succeeded");
  if (res.count > 0)
  {
    sema_print(&res);
    ASSERT(res.count == 0, "no spurious errors on clean project");
  }
  teardown(&proj, &res);
}

static void test_sema_print_formats_errors(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("e002_main_has_params", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "E002");

  int saved = dup(fileno(stderr));
  FILE *tmp = tmpfile();
  ASSERT(saved >= 0, "saved stderr");
  ASSERT(tmp != NULL, "opened temp file");

  if (saved >= 0 && tmp)
  {
    ASSERT(dup2(fileno(tmp), fileno(stderr)) >= 0, "redirected stderr");
    sema_print(&res);
    fflush(stderr);
    ASSERT(dup2(saved, fileno(stderr)) >= 0, "restored stderr");
    close(saved);

    fseek(tmp, 0, SEEK_SET);
    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';
    ASSERT(strstr(buf, "Error E002: Invalid main signature") != NULL, "printed diagnostic header");
    ASSERT(strstr(buf, "--> main.fn:1") != NULL, "printed diagnostic location");
    ASSERT(strstr(buf, "'main' must take no arguments.") != NULL, "printed diagnostic context");
    fclose(tmp);
  }

  teardown(&proj, &res);
}

static void test_t001_call_return_type(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t001_call_return_type", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "T001");
  teardown(&proj, &res);
}

static void test_t001_call_arg_type(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t001_call_arg_type", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "T001");
  teardown(&proj, &res);
}

static void test_t001_call_arg_count(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("t001_call_arg_count", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "T001");
  teardown(&proj, &res);
}
/* -------------------------------------------------------------------------
 * entry point
 * ---------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * S004 — break/continue outside loop
 * ---------------------------------------------------------------------- */

static void test_s004_break_outside_loop(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("s004_break_outside_loop", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "S004");
  teardown(&proj, &res);
}

static void test_s004_continue_outside_loop(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("s004_continue_outside_loop", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "S004");
  teardown(&proj, &res);
}

static void test_s004_not_emitted_in_loop(void)
{
  /* break inside a while must not fire S004 */
  Project proj;
  SemaResult res;
  ASSERT(load("ok_minimal", &proj, &res) == 0, "project_load succeeded");
  assert_no_error(&res, "S004");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * S005 — bare return in non-void function
 * ---------------------------------------------------------------------- */

static void test_s005_bare_return(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("s005_bare_return", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "S005");
  teardown(&proj, &res);
}

static void test_s005_not_emitted_in_void(void)
{
  /* bare return; in a void fn is legal — must not produce S005 */
  Project proj;
  SemaResult res;
  ASSERT(load("ok_minimal", &proj, &res) == 0, "project_load succeeded");
  assert_no_error(&res, "S005");
  teardown(&proj, &res);
}

/* -------------------------------------------------------------------------
 * I005 — unknown function in unqualified call
 * ---------------------------------------------------------------------- */

static void test_i005_unknown_fn(void)
{
  Project proj;
  SemaResult res;
  ASSERT(load("i005_unknown_fn", &proj, &res) == 0, "project_load succeeded");
  assert_error(&res, "I005");
  teardown(&proj, &res);
}
int main(void)
{
  /* ok — valid projects */
  run_test("ok_minimal", test_ok_minimal);
  run_test("ok_main_return_type", test_ok_main_return_type);
  run_test("ok_multi_concept", test_ok_multi_concept);
  run_test("ok_impl_file", test_ok_impl_file);
  run_test("ok_helper_fns", test_ok_helper_fns);

  /* entry point */
  run_test("e001_missing_main", test_e001_missing_main);
  run_test("e001_declared_main_missing_file", test_e001_declared_main_missing_file);
  run_test("e001_main_fn_missing_main_function", test_e001_main_fn_missing_main_function);
  run_test("e002_main_has_params", test_e002_main_has_params);
  run_test("e002_main_bad_return_type", test_e002_main_bad_return_type);

  /* file structure */
  run_test("f001_missing_primary", test_f001_missing_primary);
  run_test("f002_duplicate_primary", test_f002_duplicate_primary);
  run_test("f003_name_mismatch", test_f003_name_mismatch);

  /* concept directories */
  run_test("c001_missing_dir", test_c001_missing_dir);
  run_test("c002_undeclared_dir", test_c002_undeclared_dir);

  /* imports */
  run_test("i001_no_edge", test_i001_no_edge);
  run_test("i001_not_emitted_for_main", test_i001_not_emitted_for_main);
  run_test("i002_unknown_concept", test_i002_unknown_concept);
  run_test("i003_unknown_fn", test_i003_unknown_fn);
  run_test("i004_impl_access", test_i004_impl_access);

  /* parse error surfacing */
  run_test("s001_parse_error_surfaced", test_s001_parse_error_surfaced);
  run_test("parse_errors_not_doubled", test_parse_errors_not_doubled);
  run_test("s001_main_parse_error_not_doubled", test_s001_main_parse_error_not_doubled);

  /* types */
  run_test("t001_type_mismatch", test_t001_type_mismatch);
  run_test("t001_ptr_index_non_ptr", test_t001_ptr_index_non_ptr);
  run_test("ok_ptr_str_cast", test_ok_ptr_str_cast);
  run_test("ok_cast_conversions", test_ok_cast_conversions);
  run_test("t001_invalid_cast", test_t001_invalid_cast);
  run_test("t001_implicit_ptr_str_return", test_t001_implicit_ptr_str_return);
  run_test("t001_implicit_ptr_str_let", test_t001_implicit_ptr_str_let);
  run_test("t003_missing_return", test_t003_missing_return);
  run_test("t004_return_in_void", test_t004_return_in_void);
  run_test("t005_immutable_reassign", test_t005_immutable_reassign);
  run_test("t005_for_post_increment_immutable", test_t005_for_post_increment_immutable);
  run_test("t001_for_post_increment_type", test_t001_for_post_increment_type);

  /* regression */
  run_test("ok_no_spurious_errors", test_ok_no_spurious_errors);
  run_test("ok_for_loop", test_ok_for_loop);
  run_test("sema_print_formats_errors", test_sema_print_formats_errors);

  run_test("t001_call_return_type", test_t001_call_return_type);
  run_test("t001_call_arg_type", test_t001_call_arg_type);
  run_test("t001_call_arg_count", test_t001_call_arg_count);

  /* s004 */
  run_test("s004_break_outside_loop", test_s004_break_outside_loop);
  run_test("s004_continue_outside_loop", test_s004_continue_outside_loop);
  run_test("s004_not_emitted_in_loop", test_s004_not_emitted_in_loop);

  /* s005 */
  run_test("s005_bare_return", test_s005_bare_return);
  run_test("s005_not_emitted_in_void", test_s005_not_emitted_in_void);

  /* i005 */
  run_test("i005_unknown_fn", test_i005_unknown_fn);

  print_summary();
  return tc_suite_failed() > 0 ? 1 : 0;
}
