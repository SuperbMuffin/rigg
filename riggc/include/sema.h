#ifndef SEMA_H
#define SEMA_H

#include "project.h"

typedef struct
{
  char code[8];
  char *message;
  char *file;
  int line;
  char *context;
} SemaError;

typedef struct
{
  SemaError *errors;
  int count;
  int cap;
} SemaResult;

void sema_check(const Project *proj, SemaResult *result);
void sema_print(const SemaResult *result);
void sema_free(SemaResult *result);

#endif
