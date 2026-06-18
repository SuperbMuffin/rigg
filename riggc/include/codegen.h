#ifndef CODEGEN_H
#define CODEGEN_H

#include "project.h"

typedef struct
{
  int emit_ir_only;          /* --emit-ir: skip llc/clang after writing .ll files */
  const char *target_triple; /* LLVM target triple, e.g. "x86_64-pc-linux-gnu" */
  const char *opt_level;     /* e.g. "-O0", "-O2", etc. */
  int unsafe;                /* skip runtime bounds checks on ptr indexing */
} CodegenOptions;

/* Generate LLVM IR for the project. Writes one .ll per concept under
   <project_root>/build/ir/. Returns 0 on success, -1 on error. */
int codegen_run(const Project *proj, const CodegenOptions *opts);

#endif
