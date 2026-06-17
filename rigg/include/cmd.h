#ifndef CMD_H
#define CMD_H

int cmd_init(const char *dir);
int cmd_new(const char *name);
int cmd_build(const char *dir);
int cmd_run(const char *dir);
int cmd_check(const char *dir);

#endif
