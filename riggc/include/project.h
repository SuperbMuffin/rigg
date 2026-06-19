/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef PROJECT_H
#define PROJECT_H

#include "parser.h"
#include <stddef.h>

typedef struct
{
  int from;
  int to;
} Edge;

typedef enum
{
  FILE_FN,   /* exports exactly one public function matching the filename */
  FILE_IMPL, /* concept-private; no public exports */
} SourceFileKind;

typedef struct
{
  SourceFileKind kind;
  char *rel_path; /* e.g. "math/add.fn"; owned */
  char *stem;     /* filename without extension, e.g. "add"; owned */
  char *src;      /* file contents; owned */
  Parser parser;
  Program program;
} SourceFile;

typedef struct
{
  char *name;
  SourceFile *files;
  int file_count;
  int file_cap;
  int meta_line;
} Concept;

typedef struct
{
  Concept *concepts;
  int concept_count;
  int concept_cap;

  Edge *edges;
  int edge_count;
  int edge_cap;

  int main_concept_idx; /* index of 'main' concept, or -1 */
  char *root;
} Project;

int project_load(Project *proj, const char *root);
Concept *project_find_concept(Project *proj, const char *name, int name_len);
int project_has_edge(const Project *proj, int from_idx, int to_idx);
int project_concept_index(const Project *proj, const char *name, int name_len);
void project_free(Project *proj);

#endif
