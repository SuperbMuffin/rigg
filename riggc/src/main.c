#define _POSIX_C_SOURCE 200809L

#include "codegen.h"
#include "project.h"
#include "sema.h"
#include <stdio.h>
#include <string.h>

static int is_opt_flag(const char *arg)
{
  if (strncmp(arg, "-O", 2) == 0 && arg[2] != '\0')
    return 1;
  if (strcmp(arg, "-Ofast") == 0)
    return 1;
  return 0;
}

int main(int argc, char **argv)
{
  const char *root = ".";
  int emit_ir = 0;
  int check_only = 0;
  int unsafe = 0;
  const char *opt_level = "";

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--emit-ir") == 0)
      emit_ir = 1;
    else if (strcmp(argv[i], "--check") == 0)
      check_only = 1;
    else if (strcmp(argv[i], "--unsafe") == 0)
      unsafe = 1;
    else if (is_opt_flag(argv[i]))
      opt_level = argv[i];
    else
      root = argv[i];
  }

  Project proj;
  if (project_load(&proj, root) < 0)
    return 1;

  SemaResult result;
  sema_check(&proj, &result);

  if (result.count)
  {
    sema_print(&result);
    sema_free(&result);
    project_free(&proj);
    return 1;
  }

  sema_free(&result);

  if (check_only)
  {
    project_free(&proj);
    return 0;
  }

  char target_triple[256] = {0};
  FILE *tp = popen("clang -print-target-triple", "r");
  if (tp)
  {
    if (fgets(target_triple, sizeof(target_triple), tp))
    {
      int tlen = (int) strlen(target_triple);
      if (tlen > 0 && target_triple[tlen - 1] == '\n')
        target_triple[tlen - 1] = '\0';
    }
    pclose(tp);
  }

  CodegenOptions opts = {.emit_ir_only = emit_ir,
                         .target_triple = target_triple,
                         .opt_level = opt_level,
                         .unsafe = unsafe};
  int rc = codegen_run(&proj, &opts);

  project_free(&proj);
  return rc != 0 ? 1 : 0;
}
