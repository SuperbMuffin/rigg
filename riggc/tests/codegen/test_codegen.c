#include "codegen.h"
#include "project.h"
#include "sema.h"
#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FIXTURES_DIR
#error "FIXTURES_DIR must be defined at compile time"
#endif

/* ── helpers ─────────────────────────────────────────────────────────────── */

static const char *fix(const char *name)
{
  static char buf[1024];
  snprintf(buf, sizeof(buf), "%s/%s", FIXTURES_DIR, name);
  return buf;
}

/* Compile fixture to IR. Returns 0 on success. */
static int emit(const char *fixture)
{
  Project proj;
  if (project_load(&proj, fix(fixture)) < 0)
    return -1;

  SemaResult res;
  sema_check(&proj, &res);
  if (res.count > 0)
  {
    sema_free(&res);
    project_free(&proj);
    return -1;
  }
  sema_free(&res);

  CodegenOptions opts = {.emit_ir_only = 1};
  int rc = codegen_run(&proj, &opts);
  project_free(&proj);
  return rc;
}

/* Read entire file into a malloc'd buffer. Caller frees. Returns NULL on error. */
static char *read_file(const char *path)
{
  FILE *f = fopen(path, "r");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);
  char *buf = malloc((size_t) sz + 1);
  size_t got = fread(buf, 1, (size_t) sz, f);
  buf[got] = '\0';
  fclose(f);
  return buf;
}

static char *ir_path(const char *fixture, const char *concept)
{
  static char buf[1024];
  snprintf(buf, sizeof(buf), "%s/%s/build/ir/%s.ll", FIXTURES_DIR, fixture, concept);
  return buf;
}

static void assert_ir_contains(const char *fixture, const char *concept, const char *needle)
{
  char *src = read_file(ir_path(fixture, concept));
  ASSERT(src != NULL, "IR file exists");
  if (src)
  {
    int found = strstr(src, needle) != NULL;
    if (!found)
    {
      fprintf(stderr, "  IR missing: %s\n  in: %s/%s.ll\n", needle, fixture, concept);
      ASSERT(found, "IR contains expected pattern");
    }
    free(src);
  }
}

static void assert_ir_not_contains(const char *fixture, const char *concept, const char *needle)
{
  char *src = read_file(ir_path(fixture, concept));
  ASSERT(src != NULL, "IR file exists");
  if (src)
  {
    int found = strstr(src, needle) != NULL;
    if (found)
    {
      fprintf(stderr, "  IR should not contain: %s\n  in: %s/%s.ll\n", needle, fixture, concept);
      ASSERT(!found, "IR does not contain unexpected pattern");
    }
    free(src);
  }
}

/* ── tests ───────────────────────────────────────────────────────────────── */

static void test_fn_mangle(void)
{
  ASSERT(emit("fn_mangle") == 0, "emit succeeded");
  /* math concept: mangled name and i32 return type */
  assert_ir_contains("fn_mangle", "math", "define i32 @math__add(");
  /* main concept: mangled correctly, declares math__add */
  assert_ir_contains("fn_mangle", "main", "declare i32 @math__add(");
  assert_ir_contains("fn_mangle", "main", "@main(");
}

static void test_fn_void_main(void)
{
  ASSERT(emit("fn_void_main") == 0, "emit succeeded");
  /* void main() in Rigg must become define i32 @main in IR */
  assert_ir_contains("fn_void_main", "main", "define i32 @main(");
  /* implicit ret i32 0 */
  assert_ir_contains("fn_void_main", "main", "ret i32 0");
  /* must not emit ret void */
  assert_ir_not_contains("fn_void_main", "main", "ret void");
}

static void test_let_alloca(void)
{
  ASSERT(emit("let_alloca") == 0, "emit succeeded");
  assert_ir_contains("let_alloca", "main", "alloca i32");
  assert_ir_contains("let_alloca", "main", "store i32");
  assert_ir_contains("let_alloca", "main", "load i32");
}

static void test_arithmetic(void)
{
  ASSERT(emit("arithmetic") == 0, "emit succeeded");
  assert_ir_contains("arithmetic", "main", "add i32");
  assert_ir_contains("arithmetic", "main", "sub i32");
  assert_ir_contains("arithmetic", "main", "mul i32");
  assert_ir_contains("arithmetic", "main", "sdiv i32");
  assert_ir_contains("arithmetic", "main", "srem i32");
}

static void test_if_else(void)
{
  ASSERT(emit("if_else") == 0, "emit succeeded");
  /* conditional branch */
  assert_ir_contains("if_else", "main", "br i1");
  /* at least two unconditional branches (then→end, else→end) */
  assert_ir_contains("if_else", "main", "br label");
}

static void test_while_loop(void)
{
  ASSERT(emit("while_loop") == 0, "emit succeeded");
  /* loop header branch */
  assert_ir_contains("while_loop", "main", "br i1");
  /* back-edge: unconditional branch back to header */
  assert_ir_contains("while_loop", "main", "br label");
  assert_ir_contains("while_loop", "main", "icmp slt");
}

static void test_for_loop(void)
{
  ASSERT(emit("for_loop") == 0, "emit succeeded");
  assert_ir_contains("for_loop", "main", "br i1");
  assert_ir_contains("for_loop", "main", "icmp slt");
  assert_ir_contains("for_loop", "main", "add i32");
  assert_ir_contains("for_loop", "main", "store i32");
}

static void test_loop_break(void)
{
  ASSERT(emit("loop_break") == 0, "emit succeeded");
  /* unconditional branch to header on entry */
  assert_ir_contains("loop_break", "main", "br label");
  /* break emits branch to exit label */
  assert_ir_contains("loop_break", "main", "ret i32");
}

static void test_qual_call(void)
{
  ASSERT(emit("qual_call") == 0, "emit succeeded");
  /* main.ll declares math__double */
  assert_ir_contains("qual_call", "main", "declare i32 @math__double(");
  /* main.ll calls it */
  assert_ir_contains("qual_call", "main", "call i32 @math__double(");
  /* math.ll defines it */
  assert_ir_contains("qual_call", "math", "define i32 @math__double(");
}

static void test_float_ops(void)
{
  ASSERT(emit("float_ops") == 0, "emit succeeded");
  assert_ir_contains("float_ops", "main", "alloca double");
  assert_ir_contains("float_ops", "main", "fadd double");
}

static void test_variadic_str(void)
{
  ASSERT(emit("variadic_str") == 0, "emit succeeded");
  /* The second argument ("world") should be ptr, not i32 */
  assert_ir_contains("variadic_str", "main", "call i32 @printf(ptr %1, ptr %2)");
}

static void test_ptr_type(void)
{
  ASSERT(emit("ptr_type") == 0, "emit succeeded");
  assert_ir_contains("ptr_type", "main", "define ptr @main__test_ptr(ptr %p0)");
}

static int emit_unsafe(const char *fixture)
{
  Project proj;
  if (project_load(&proj, fix(fixture)) < 0)
    return -1;

  SemaResult res;
  sema_check(&proj, &res);
  if (res.count > 0)
  {
    sema_free(&res);
    project_free(&proj);
    return -1;
  }
  sema_free(&res);

  CodegenOptions opts = {.emit_ir_only = 1, .unsafe = 1};
  int rc = codegen_run(&proj, &opts);
  project_free(&proj);
  return rc;
}

static void test_ptr_index(void)
{
  ASSERT(emit_unsafe("ptr_index") == 0, "emit succeeded");
  assert_ir_contains("ptr_index", "main", "getelementptr i8, ptr");
  assert_ir_contains("ptr_index", "main", "load i8, ptr");
  assert_ir_contains("ptr_index", "main", "store i8");
}

static void test_ptr_str_cast(void)
{
  ASSERT(emit("ptr_str_cast") == 0, "emit succeeded");
  assert_ir_contains("ptr_str_cast", "main", "define ptr @main__ptr_to_str(ptr %p0)");
  assert_ir_contains("ptr_str_cast", "main", "define ptr @main__str_to_ptr(ptr %p0)");
  assert_ir_contains("ptr_str_cast", "main", "define ptr @main__buffer_as_str(ptr %p0)");
  assert_ir_contains("ptr_str_cast", "main", "ret ptr %2");
  assert_ir_not_contains("ptr_str_cast", "main", "call ptr @rigg_");
}

static void test_cast_int(void)
{
  ASSERT(emit("cast_int") == 0, "emit succeeded");
  assert_ir_contains("cast_int", "main", "sext i32");
  assert_ir_contains("cast_int", "main", "trunc i64");
}

static void test_cast_str(void)
{
  ASSERT(emit("cast_str") == 0, "emit succeeded");
  assert_ir_contains("cast_str", "main", "call i32 @rigg_str_to_i32(ptr");
  assert_ir_contains("cast_str", "main", "call ptr @rigg_i32_to_str(i32");
  assert_ir_contains("cast_str", "main", "call ptr @rigg_bool_to_str(i1");
}

static void test_numeric_edges(void)
{
  ASSERT(emit("numeric_edges") == 0, "emit succeeded");
  assert_ir_contains("numeric_edges", "main", "udiv i32");
  assert_ir_contains("numeric_edges", "main", "urem i32");
  assert_ir_contains("numeric_edges", "main", "icmp uge i32");
  assert_ir_contains("numeric_edges", "main", "icmp ult i32");
  assert_ir_contains("numeric_edges", "main", "fdiv float");
  assert_ir_contains("numeric_edges", "main", "fcmp ole float");
  assert_ir_contains("numeric_edges", "main", "fneg float");
  assert_ir_contains("numeric_edges", "main", "inttoptr i32");
  assert_ir_contains("numeric_edges", "main", "ptrtoint ptr");
  assert_ir_contains("numeric_edges", "main", "call i1 @rigg_str_to_bool(ptr");
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void)
{
  run_test("fn_mangle", test_fn_mangle);
  run_test("fn_void_main", test_fn_void_main);
  run_test("let_alloca", test_let_alloca);
  run_test("arithmetic", test_arithmetic);
  run_test("if_else", test_if_else);
  run_test("while_loop", test_while_loop);
  run_test("for_loop", test_for_loop);
  run_test("loop_break", test_loop_break);
  run_test("qual_call", test_qual_call);
  run_test("float_ops", test_float_ops);
  run_test("variadic_str", test_variadic_str);
  run_test("ptr_type", test_ptr_type);
  run_test("ptr_index", test_ptr_index);
  run_test("ptr_str_cast", test_ptr_str_cast);
  run_test("cast_int", test_cast_int);
  run_test("cast_str", test_cast_str);
  run_test("numeric_edges", test_numeric_edges);

  print_summary();
  return tc_suite_failed() > 0 ? 1 : 0;
}
