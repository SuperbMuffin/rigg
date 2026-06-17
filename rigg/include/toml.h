#ifndef TOML_H
#define TOML_H

typedef struct
{
  char name[256];
  char version[64];
  char author[256];
} ProjectToml;

/* Parse project.toml in the given project root.
   Returns 0 on success, -1 on error. */
int toml_load(const char *root, ProjectToml *out);

#endif
