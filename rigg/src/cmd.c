#define _POSIX_C_SOURCE 200809L

#include "cmd.h"
#include "toml.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int mkdir_p(const char *path)
{
  char tmp[4096];
  snprintf(tmp, sizeof(tmp), "%s", path);
  for (char *p = tmp + 1; *p; p++)
  {
    if (*p == '/')
    {
      *p = '\0';
      if (mkdir(tmp, 0755) < 0 && errno != EEXIST)
      {
        fprintf(stderr, "rigg: cannot create directory '%s': %s\n", tmp, strerror(errno));
        return -1;
      }
      *p = '/';
    }
  }
  if (mkdir(tmp, 0755) < 0 && errno != EEXIST)
  {
    fprintf(stderr, "rigg: cannot create directory '%s': %s\n", tmp, strerror(errno));
    return -1;
  }
  return 0;
}

static int write_file(const char *path, const char *content)
{
  FILE *f = fopen(path, "wx");
  if (!f)
  {
    fprintf(stderr, "rigg: '%s' already exists\n", path);
    return -1;
  }
  fputs(content, f);
  fclose(f);
  return 0;
}

static int is_dir(const char *path)
{
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_file(const char *path)
{
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int run_riggc(const char *dir, int check_only)
{
  char cmd[4096];
  if (check_only)
    snprintf(cmd, sizeof(cmd), "riggc --check %s", dir);
  else
    snprintf(cmd, sizeof(cmd), "riggc %s", dir);

  int rc = system(cmd);
  if (rc != 0)
  {
    fprintf(stderr, "rigg: build failed\n");
    return -1;
  }
  return 0;
}

/* Scaffold the project files into an existing directory */
static int scaffold(const char *dir, const char *name)
{
  char path[4096];

  snprintf(path, sizeof(path), "%s/project.toml", dir);
  char toml[512];
  snprintf(toml, sizeof(toml),
           "name = \"%s\"\n"
           "version = \"0.1.0\"\n"
           "author = \"\"\n",
           name);
  if (write_file(path, toml) < 0)
    return -1;

  snprintf(path, sizeof(path), "%s/project.meta", dir);
  if (write_file(path, "main\n") < 0)
    return -1;

  snprintf(path, sizeof(path), "%s/main.fn", dir);
  if (write_file(path, "fn main()\n{\n}\n") < 0)
    return -1;

  return 0;
}

int cmd_init(const char *dir)
{
  /* Derive a name from the directory */
  const char *name = strrchr(dir, '/');
  name = name ? name + 1 : dir;
  if (strcmp(name, ".") == 0)
  {
    /* Use cwd name */
    static char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)))
    {
      const char *p = strrchr(cwd, '/');
      name = p ? p + 1 : cwd;
    }
    else
      name = "project";
  }

  if (scaffold(dir, name) < 0)
    return 1;

  printf("initialized project '%s'\n", name);
  return 0;
}

int cmd_new(const char *name)
{
  char dir[4096];
  snprintf(dir, sizeof(dir), "%s/src", name);

  if (is_dir(name))
  {
    fprintf(stderr, "rigg: directory '%s' already exists\n", name);
    return 1;
  }

  if (mkdir_p(dir) < 0)
    return 1;

  /* README */
  char readme_path[4096];
  snprintf(readme_path, sizeof(readme_path), "%s/README.md", name);
  char readme[512];
  snprintf(readme, sizeof(readme), "# %s\n", name);
  if (write_file(readme_path, readme) < 0)
    return 1;

  if (scaffold(dir, name) < 0)
    return 1;

  printf("created project '%s'\n", name);
  return 0;
}

int cmd_build(const char *dir)
{
  char toml_path[4096];
  snprintf(toml_path, sizeof(toml_path), "%s/project.toml", dir);

  /* Walk up to find project root if not in root */
  if (!is_file(toml_path))
  {
    fprintf(stderr, "rigg: no project.toml found\n");
    return 1;
  }

  ProjectToml toml;
  if (toml_load(dir, &toml) < 0)
    return 1;

  if (run_riggc(dir, 0) < 0)
    return 1;

  /* Rename build/out to build/<name> */
  char out_path[4096];
  char named_path[4096];
  snprintf(out_path, sizeof(out_path), "%s/build/out", dir);
  snprintf(named_path, sizeof(named_path), "%s/build/%s", dir, toml.name);

  if (is_file(out_path))
  {
    if (rename(out_path, named_path) < 0)
    {
      fprintf(stderr, "rigg: cannot rename output: %s\n", strerror(errno));
      return 1;
    }
  }

  printf("built '%s'\n", toml.name);
  return 0;
}

int cmd_run(const char *dir)
{
  char toml_path[4096];
  snprintf(toml_path, sizeof(toml_path), "%s/project.toml", dir);

  if (!is_file(toml_path))
  {
    fprintf(stderr, "rigg: no project.toml found\n");
    return 1;
  }

  ProjectToml toml;
  if (toml_load(dir, &toml) < 0)
    return 1;

  if (cmd_build(dir) != 0)
    return 1;

  char bin[4096];
  snprintf(bin, sizeof(bin), "%s/build/%s", dir, toml.name);

  if (!is_file(bin))
  {
    fprintf(stderr, "rigg: binary '%s' not found after build\n", bin);
    return 1;
  }

  char cmd[4096];
  snprintf(cmd, sizeof(cmd), "%s", bin);
  return system(cmd) != 0 ? 1 : 0;
}

int cmd_check(const char *dir)
{
  char toml_path[4096];
  snprintf(toml_path, sizeof(toml_path), "%s/project.toml", dir);

  if (!is_file(toml_path))
  {
    fprintf(stderr, "rigg: no project.toml found\n");
    return 1;
  }

  return run_riggc(dir, 1) < 0 ? 1 : 0;
}
