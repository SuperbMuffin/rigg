#ifndef CMD_H
#define CMD_H

typedef struct
{
  int emit_ir;
  int unsafe;
} BuildFlags;

int cmd_init(const char *dir);
int cmd_new(const char *name);
int cmd_build(const char *dir, BuildFlags flags);
int cmd_run(const char *dir, BuildFlags flags);
int cmd_check(const char *dir);

#endif
