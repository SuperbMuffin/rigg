#include "cmd.h"
#include "toml.h"

#include <stdio.h>
#include <string.h>

static void usage(void)
{
  fprintf(stderr, "usage: rigg <command> [args]\n"
                  "\n"
                  "commands:\n"
                  "  init            initialise a new project in the current directory\n"
                  "  new <name>      create a new project directory\n"
                  "  build           compile the current project\n"
                  "  run             build and execute the current project\n"
                  "  check           check for errors without producing output\n");
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
    return cmd_build(".");

  if (strcmp(cmd, "run") == 0)
    return cmd_run(".");

  if (strcmp(cmd, "check") == 0)
    return cmd_check(".");

  fprintf(stderr, "rigg: unknown command '%s'\n", cmd);
  usage();
  return 1;
}
