#include "cmd.h"
#include "toml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
  fprintf(stderr, "usage: rigg <command> [args]\n"
                  "\n"
                  "commands:\n"
                  "  init                  initialise a new project in the current directory\n"
                  "  new <name>            create a new project directory\n"
                  "  build [--emit-ir] [--unsafe]\n"
                  "                        compile the current project\n"
                  "  run [--emit-ir] [--unsafe]\n"
                  "                        build and execute the current project\n"
                  "  check                 check for errors without producing output\n"
                  "\n"
                  "build options in project.toml:\n"
                  "  opt = 0               clang optimization level (-O0 .. -O3, s, z, g, fast)\n"
                  "\n"
                  "flags (--emit-ir, --unsafe) are CLI-only and forwarded to riggc\n");
}

static BuildFlags parse_build_flags(int argc, char **argv, int start)
{
  BuildFlags flags = {0};
  for (int i = start; i < argc; i++)
  {
    if (strcmp(argv[i], "--emit-ir") == 0)
      flags.emit_ir = 1;
    else if (strcmp(argv[i], "--unsafe") == 0)
      flags.unsafe = 1;
    else
    {
      fprintf(stderr, "rigg: unknown flag '%s'\n", argv[i]);
      usage();
      exit(1);
    }
  }
  return flags;
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    usage();
    return 1;
  }

  const char *cmd = argv[1];

  if (strcmp(cmd, "init") == 0)
    return cmd_init(".");

  if (strcmp(cmd, "new") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "rigg: 'new' requires a project name\n");
      return 1;
    }
    return cmd_new(argv[2]);
  }

  if (strcmp(cmd, "build") == 0)
    return cmd_build(".", parse_build_flags(argc, argv, 2));

  if (strcmp(cmd, "run") == 0)
    return cmd_run(".", parse_build_flags(argc, argv, 2));

  if (strcmp(cmd, "check") == 0)
    return cmd_check(".");

  fprintf(stderr, "rigg: unknown command '%s'\n", cmd);
  usage();
  return 1;
}
