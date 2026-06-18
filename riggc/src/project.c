#include "project.h"
#include "diag.h"
#include "parser.h"
#include "util.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ARENA_SIZE (1024 * 1024)

/* Short aliases */
#define xmalloc util_xmalloc
#define xrealloc util_xrealloc
#define xstrdup util_xstrdup

static char *path_join(const char *a, const char *b)
{
  size_t la = strlen(a), lb = strlen(b);
  char *out = xmalloc(la + 1 + lb + 1);
  memcpy(out, a, la);
  out[la] = '/';
  memcpy(out + la + 1, b, lb + 1);
  return out;
}

static int is_file(const char *path)
{
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int is_concept_name(const char *name)
{
  if (!name || !islower((unsigned char) name[0]))
    return 0;
  for (const char *p = name + 1; *p; p++)
    if (!islower((unsigned char) *p) && !isdigit((unsigned char) *p) && *p != '_')
      return 0;
  return 1;
}

static char *read_file(const char *path)
{
  FILE *f = fopen(path, "r");
  if (!f)
  {
    fprintf(stderr, "rigg: cannot open '%s': %s\n", path, strerror(errno));
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);
  char *buf = xmalloc((size_t) sz + 1);
  size_t got = fread(buf, 1, (size_t) sz, f);
  buf[got] = '\0';
  fclose(f);
  return buf;
}

static void push_concept(Project *proj, Concept c)
{
  if (proj->concept_count == proj->concept_cap)
  {
    proj->concept_cap = proj->concept_cap ? proj->concept_cap * 2 : 8;
    proj->concepts = xrealloc(proj->concepts, (size_t) proj->concept_cap * sizeof(Concept));
  }
  proj->concepts[proj->concept_count++] = c;
}

static void push_edge(Project *proj, Edge e)
{
  if (proj->edge_count == proj->edge_cap)
  {
    proj->edge_cap = proj->edge_cap ? proj->edge_cap * 2 : 16;
    proj->edges = xrealloc(proj->edges, (size_t) proj->edge_cap * sizeof(Edge));
  }
  proj->edges[proj->edge_count++] = e;
}

static void push_file(Concept *c, SourceFile sf)
{
  if (c->file_count == c->file_cap)
  {
    c->file_cap = c->file_cap ? c->file_cap * 2 : 4;
    c->files = xrealloc(c->files, (size_t) c->file_cap * sizeof(SourceFile));
  }
  c->files[c->file_count++] = sf;
}

/* Two-pass parse: first pass builds the concept list, second resolves edges.
   Edges can reference concepts declared later in the file, so we need the
   full concept list before we can look up target indices. */
static int parse_meta(Project *proj)
{
  char *meta_path = path_join(proj->root, "project.meta");
  FILE *f = fopen(meta_path, "r");
  free(meta_path);

  if (!f)
  {
    fprintf(stderr, "rigg: project.meta not found in '%s'\n", proj->root);
    return -1;
  }

  /* NOTE: lines longer than 511 bytes will be silently truncated by fgets.
     This is unlikely in practice for project.meta but worth fixing if the
     format is ever extended to support longer values (e.g. inline comments). */
  char line[512];
  int lineno = 0;
  int current_idx = -1;
  int ok = 1;

  while (fgets(line, sizeof(line), f))
  {
    lineno++;

    char *comment = strchr(line, '#');
    if (comment)
      *comment = '\0';

    int len = (int) strlen(line);
    while (len > 0 && isspace((unsigned char) line[len - 1]))
      line[--len] = '\0';
    if (len == 0)
      continue;

    char *trimmed = line;
    while (*trimmed == ' ' || *trimmed == '\t')
      trimmed++;

    if (strncmp(trimmed, "->", 2) == 0)
    {
      char *dep = trimmed + 2;
      while (*dep == ' ' || *dep == '\t')
        dep++;
      int dlen = (int) strlen(dep);
      if (dlen > 0 && dep[dlen - 1] == ':')
        dep[--dlen] = '\0';

      if (!is_concept_name(dep))
      {
        fprintf(stderr, "rigg: project.meta:%d: invalid dependency name '%s'\n", lineno, dep);
        ok = 0;
      }
      else if (current_idx < 0)
      {
        fprintf(stderr, "rigg: project.meta:%d: '->' before any concept declaration\n", lineno);
        ok = 0;
      }
      continue;
    }

    char name[256];
    int nlen = 0;
    while (trimmed[nlen] && trimmed[nlen] != ':' && trimmed[nlen] != ' ' && trimmed[nlen] != '\t')
      nlen++;
    if (nlen == 0 || nlen >= (int) sizeof(name))
    {
      fprintf(stderr, "rigg: project.meta:%d: unexpected token\n", lineno);
      ok = 0;
      continue;
    }
    memcpy(name, trimmed, (size_t) nlen);
    name[nlen] = '\0';

    if (!is_concept_name(name))
    {
      fprintf(stderr, "rigg: project.meta:%d: invalid concept name '%s'\n", lineno, name);
      ok = 0;
      continue;
    }

    int dup = 0;
    for (int i = 0; i < proj->concept_count; i++)
    {
      if (strcmp(proj->concepts[i].name, name) == 0)
      {
        fprintf(stderr,
                "rigg: project.meta:%d: duplicate concept '%s' (first declared "
                "on line %d)\n",
                lineno, name, proj->concepts[i].meta_line);
        ok = 0;
        dup = 1;
        current_idx = i;
        break;
      }
    }
    if (dup)
      continue;

    Concept c = {0};
    c.name = xstrdup(name);
    c.meta_line = lineno;
    current_idx = proj->concept_count;
    push_concept(proj, c);
  }

  fclose(f);
  if (!ok)
    return -1;

  /* Second pass: resolve edges now that all concepts are known. */
  meta_path = path_join(proj->root, "project.meta");
  f = fopen(meta_path, "r");
  free(meta_path);
  if (!f)
    return -1;

  lineno = 0;
  current_idx = -1;

  while (fgets(line, sizeof(line), f))
  {
    lineno++;
    char *comment = strchr(line, '#');
    if (comment)
      *comment = '\0';
    int len = (int) strlen(line);
    while (len > 0 && isspace((unsigned char) line[len - 1]))
      line[--len] = '\0';
    if (len == 0)
      continue;

    char *trimmed = line;
    while (*trimmed == ' ' || *trimmed == '\t')
      trimmed++;

    if (strncmp(trimmed, "->", 2) == 0)
    {
      char dep[256];
      char *src = trimmed + 2;
      while (*src == ' ' || *src == '\t')
        src++;
      int dlen = (int) strlen(src);
      if (dlen > 0 && src[dlen - 1] == ':')
        src[--dlen] = '\0';
      if (dlen == 0 || dlen >= (int) sizeof(dep))
        continue;
      memcpy(dep, src, (size_t) dlen);
      dep[dlen] = '\0';

      if (current_idx < 0)
        continue;

      int to = project_concept_index(proj, dep, (int) strlen(dep));
      if (to < 0)
      {
        fprintf(stderr, "rigg: project.meta:%d: unknown concept '%s'\n", lineno, dep);
        ok = 0;
        continue;
      }
      Edge e = {current_idx, to};
      push_edge(proj, e);
      continue;
    }

    char name[256];
    int nlen = 0;
    while (trimmed[nlen] && trimmed[nlen] != ':' && trimmed[nlen] != ' ' && trimmed[nlen] != '\t')
      nlen++;
    if (nlen == 0 || nlen >= (int) sizeof(name))
      continue;
    memcpy(name, trimmed, (size_t) nlen);
    name[nlen] = '\0';
    current_idx = project_concept_index(proj, name, nlen);
  }

  fclose(f);
  return ok ? 0 : -1;
}

/* Iterative DFS cycle detection. Colours: 0=white, 1=gray (in stack), 2=black
   (done). On finding a back-edge we print the cycle path and return -1. We
   avoid recursion to stay safe on arbitrarily deep graphs. */
static int detect_cycles(const Project *proj)
{
  int n = proj->concept_count;
  if (n == 0)
    return 0;

  int *colour = xmalloc((size_t) n * sizeof(int));
  memset(colour, 0, (size_t) n * sizeof(int));

  /* path tracks the current DFS stack for printing cycle members */
  int *path = xmalloc((size_t) n * sizeof(int));
  /* stack entries: (node, next_child_index) */
  int *stk_node = xmalloc((size_t) n * sizeof(int));
  int *stk_child = xmalloc((size_t) n * sizeof(int));

  int found = 0;

  for (int start = 0; start < n && !found; start++)
  {
    if (colour[start] != 0)
      continue;

    int depth = 0;
    stk_node[0] = start;
    stk_child[0] = 0;
    colour[start] = 1;
    path[0] = start;

    while (depth >= 0 && !found)
    {
      int node = stk_node[depth];

      /* find the next unvisited child edge */
      int advanced = 0;
      while (stk_child[depth] < proj->edge_count)
      {
        int ei = stk_child[depth]++;
        if (proj->edges[ei].from != node)
          continue;
        int nb = proj->edges[ei].to;

        if (colour[nb] == 1)
        {
          /* back-edge: find where nb appears in path and print */
          diag_print_error(stderr, "G002", "Circular dependency detected");
          int ci = 0;
          while (ci <= depth && path[ci] != nb)
            ci++;
          const char *cycle[32];
          int cycle_len = 0;
          for (int pi = ci; pi <= depth && cycle_len < 32; pi++)
            cycle[cycle_len++] = proj->concepts[path[pi]].name;
          if (cycle_len < 32)
            cycle[cycle_len++] = proj->concepts[nb].name;
          diag_print_cycle(stderr, cycle, cycle_len);
          found = 1;
          break;
        }

        if (colour[nb] == 0)
        {
          colour[nb] = 1;
          depth++;
          stk_node[depth] = nb;
          stk_child[depth] = 0;
          path[depth] = nb;
          advanced = 1;
          break;
        }
      }

      if (!found && !advanced)
      {
        colour[node] = 2;
        depth--;
      }
    }
  }

  free(colour);
  free(path);
  free(stk_node);
  free(stk_child);
  return found ? -1 : 0;
}

static SourceFile parse_source_file(const char *abs_path, const char *rel_path, SourceFileKind kind)
{
  SourceFile sf = {0};
  sf.kind = kind;
  sf.rel_path = xstrdup(rel_path);

  const char *slash = strrchr(rel_path, '/');
  const char *base = slash ? slash + 1 : rel_path;
  const char *dot = strrchr(base, '.');
  size_t stem_len = dot ? (size_t) (dot - base) : strlen(base);
  char *stem_buf = xmalloc(stem_len + 1);
  memcpy(stem_buf, base, stem_len);
  stem_buf[stem_len] = '\0';
  sf.stem = stem_buf;

  sf.src = read_file(abs_path);
  if (!sf.src)
    return sf;

  parser_init(&sf.parser, sf.src, ARENA_SIZE);
  sf.program = parser_run(&sf.parser);
  return sf;
}

static void load_concept_files(Project *proj, Concept *concept)
{
  /* 'main' maps to root/main.fn, not a directory */
  if (strcmp(concept->name, "main") == 0)
  {
    char *main_path = path_join(proj->root, "main.fn");
    if (is_file(main_path))
    {
      SourceFile sf = parse_source_file(main_path, "main.fn", FILE_FN);
      push_file(concept, sf);
    }
    free(main_path);
    return;
  }

  char *concept_dir = path_join(proj->root, concept->name);
  DIR *d = opendir(concept_dir);
  if (!d)
  {
    /* Missing directory is a C001 error reported by sema, not fatal here */
    free(concept_dir);
    return;
  }

  char **names = NULL;
  int ncount = 0, ncap = 0;
  struct dirent *ent;

  while ((ent = readdir(d)) != NULL)
  {
    const char *name = ent->d_name;
    size_t nlen = strlen(name);
    int is_fn = nlen > 3 && strcmp(name + nlen - 3, ".fn") == 0;
    int is_impl = nlen > 5 && strcmp(name + nlen - 5, ".impl") == 0;
    if (!is_fn && !is_impl)
      continue;
    if (ncount == ncap)
    {
      ncap = ncap ? ncap * 2 : 8;
      names = xrealloc(names, (size_t) ncap * sizeof(char *));
    }
    names[ncount++] = xstrdup(name);
  }
  closedir(d);

  /* Insertion sort for deterministic order; concept dirs are small */
  for (int i = 1; i < ncount; i++)
  {
    char *key = names[i];
    int j = i - 1;
    while (j >= 0 && strcmp(names[j], key) > 0)
    {
      names[j + 1] = names[j];
      j--;
    }
    names[j + 1] = key;
  }

  for (int i = 0; i < ncount; i++)
  {
    const char *fname = names[i];
    size_t flen = strlen(fname);
    SourceFileKind kind = (strcmp(fname + flen - 3, ".fn") == 0) ? FILE_FN : FILE_IMPL;

    char *abs_path = path_join(concept_dir, fname);
    size_t rlen = strlen(concept->name) + 1 + flen;
    char *rel = xmalloc(rlen + 1);
    snprintf(rel, rlen + 1, "%s/%s", concept->name, fname);

    SourceFile sf = parse_source_file(abs_path, rel, kind);
    push_file(concept, sf);

    free(abs_path);
    free(rel);
    free(names[i]);
  }

  free(names);
  free(concept_dir);
}

int project_load(Project *proj, const char *root)
{
  memset(proj, 0, sizeof(*proj));
  proj->root = xstrdup(root);
  proj->main_concept_idx = -1;

  if (parse_meta(proj) < 0)
    return -1;

  if (detect_cycles(proj) < 0)
    return -1;

  proj->main_concept_idx = project_concept_index(proj, "main", 4);

  for (int i = 0; i < proj->concept_count; i++)
    load_concept_files(proj, &proj->concepts[i]);

  return 0;
}

Concept *project_find_concept(Project *proj, const char *name, int name_len)
{
  int idx = project_concept_index(proj, name, name_len);
  return idx >= 0 ? &proj->concepts[idx] : NULL;
}

int project_concept_index(const Project *proj, const char *name, int name_len)
{
  for (int i = 0; i < proj->concept_count; i++)
  {
    if ((int) strlen(proj->concepts[i].name) == name_len &&
        memcmp(proj->concepts[i].name, name, (size_t) name_len) == 0)
      return i;
  }
  return -1;
}

int project_has_edge(const Project *proj, int from_idx, int to_idx)
{
  for (int i = 0; i < proj->edge_count; i++)
    if (proj->edges[i].from == from_idx && proj->edges[i].to == to_idx)
      return 1;
  return 0;
}

void project_free(Project *proj)
{
  for (int i = 0; i < proj->concept_count; i++)
  {
    Concept *c = &proj->concepts[i];
    for (int j = 0; j < c->file_count; j++)
    {
      SourceFile *sf = &c->files[j];
      parser_free(&sf->parser);
      free(sf->src);
      free(sf->rel_path);
      free(sf->stem);
    }
    free(c->files);
    free(c->name);
  }
  free(proj->concepts);
  free(proj->edges);
  free(proj->root);
  memset(proj, 0, sizeof(*proj));
}
