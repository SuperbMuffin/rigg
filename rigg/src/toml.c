#include "toml.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static char *path_join(const char *a, const char *b, char *buf, size_t n)
{
  snprintf(buf, n, "%s/%s", a, b);
  return buf;
}

static void trim(char *s)
{
  int len = (int) strlen(s);
  while (len > 0 && isspace((unsigned char) s[len - 1]))
    s[--len] = '\0';
  char *p = s;
  while (*p && isspace((unsigned char) *p))
    p++;
  if (p != s)
    memmove(s, p, strlen(p) + 1);
}

static void strip_quotes(char *s)
{
  int len = (int) strlen(s);
  if (len >= 2 && s[0] == '"' && s[len - 1] == '"')
  {
    memmove(s, s + 1, (size_t) len - 1);
    s[len - 2] = '\0';
  }
}

int toml_load(const char *root, ProjectToml *out)
{
  char path[4096];
  path_join(root, "project.toml", path, sizeof(path));

  FILE *f = fopen(path, "r");
  if (!f)
  {
    fprintf(stderr, "rigg: cannot open '%s': %s\n", path, strerror(errno));
    return -1;
  }

  memset(out, 0, sizeof(*out));

  char line[512];
  while (fgets(line, sizeof(line), f))
  {
    char *comment = strchr(line, '#');
    if (comment)
      *comment = '\0';

    char *eq = strchr(line, '=');
    if (!eq)
      continue;

    char key[128], val[256];
    size_t klen = (size_t) (eq - line);
    if (klen >= sizeof(key))
      continue;
    memcpy(key, line, klen);
    key[klen] = '\0';
    trim(key);

    strncpy(val, eq + 1, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    trim(val);
    strip_quotes(val);

    if (strcmp(key, "name") == 0)
      strncpy(out->name, val, sizeof(out->name) - 1);
    else if (strcmp(key, "version") == 0)
      strncpy(out->version, val, sizeof(out->version) - 1);
    else if (strcmp(key, "author") == 0)
      strncpy(out->author, val, sizeof(out->author) - 1);
  }

  fclose(f);

  if (out->name[0] == '\0')
  {
    fprintf(stderr, "rigg: project.toml is missing 'name'\n");
    return -1;
  }

  return 0;
}
