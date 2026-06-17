#include "sema.h"
#include "parser.h"
#include "project.h"
#include "util.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Short aliases so call sites stay readable */
#define xmalloc util_xmalloc
#define xstrdup util_xstrdup
#define xsprintf util_xsprintf

/* push_error owns message and context (pass xstrdup/xsprintf results directly).
   file is strdup'd internally. */
static void push_error(SemaResult *res, const char *code, const char *file, int line, char *context,
                       char *message)
{
  if (res->count == res->cap)
  {
    res->cap = res->cap ? res->cap * 2 : 8;
    res->errors = util_xrealloc(res->errors, (size_t) res->cap * sizeof(SemaError));
    if (!res->errors)
    {
      fprintf(stderr, "rigg: out of memory\n");
      exit(1);
    }
  }
  SemaError *e = &res->errors[res->count++];
  strncpy(e->code, code, sizeof(e->code) - 1);
  e->code[sizeof(e->code) - 1] = '\0';
  e->message = message;
  e->file = xstrdup(file);
  e->line = line;
  e->context = context;
}

static void check_parse_errors(const SourceFile *sf, SemaResult *res)
{
  for (int i = 0; i < sf->parser.error_count; i++)
  {
    const ParseError *pe = &sf->parser.errors[i];
    push_error(res, pe->code[0] ? pe->code : "S001", sf->rel_path, pe->line, NULL,
               xstrdup(pe->message));
  }
}

static void check_entry_point(const Project *proj, SemaResult *res)
{
  if (proj->main_concept_idx < 0)
  {
    push_error(res, "E001", NULL, 0,
               xstrdup("No 'main' concept found in project.meta.\n"
                       "  Add 'main:' with its dependencies and a root-level main.fn."),
               xstrdup("Missing entry point"));
    return;
  }

  const Concept *main_concept = &proj->concepts[proj->main_concept_idx];

  /* load_concept_files leaves file_count == 0 when main.fn is absent */
  if (main_concept->file_count == 0)
  {
    push_error(res, "E001", "project.meta", main_concept->meta_line,
               xstrdup("'main' is declared in project.meta but main.fn was not found."),
               xstrdup("Missing entry point"));
    return;
  }

  const SourceFile *sf = &main_concept->files[0];
  check_parse_errors(sf, res);

  const FnDecl *main_fn = NULL;
  for (int i = 0; i < sf->program.fn_count; i++)
  {
    const FnDecl *fn = &sf->program.fns[i];
    if (fn->name_len == 4 && memcmp(fn->name, "main", 4) == 0)
      main_fn = fn;
  }

  if (!main_fn)
  {
    push_error(res, "F001", "main.fn", 0, xstrdup("Expected a function named 'main'."),
               xstrdup("Missing primary function in 'main.fn'"));
    return;
  }

  if (main_fn->param_count > 0)
  {
    push_error(res, "E002", "main.fn", main_fn->line,
               xstrdup("'main' must take no arguments.\n"
                       "  Expected: fn main() or fn main() -> i32"),
               xstrdup("Invalid main signature"));
  }

  if (main_fn->return_type != TYPE_VOID && main_fn->return_type != TYPE_I32)
  {
    push_error(res, "E002", "main.fn", main_fn->line,
               xsprintf("'main' may only return i32 or nothing, found '%s'.",
                        main_fn->return_type == TYPE_I8     ? "i8"
                        : main_fn->return_type == TYPE_I16  ? "i16"
                        : main_fn->return_type == TYPE_I64  ? "i64"
                        : main_fn->return_type == TYPE_U8   ? "u8"
                        : main_fn->return_type == TYPE_U16  ? "u16"
                        : main_fn->return_type == TYPE_U32  ? "u32"
                        : main_fn->return_type == TYPE_U64  ? "u64"
                        : main_fn->return_type == TYPE_F32  ? "f32"
                        : main_fn->return_type == TYPE_F64  ? "f64"
                        : main_fn->return_type == TYPE_BOOL ? "bool"
                        : main_fn->return_type == TYPE_STR  ? "str"
                        : main_fn->return_type == TYPE_PTR  ? "ptr"
                                                            : "?"),
               xstrdup("Invalid main signature"));
  }
}

/* Primary function = last declared function; helpers go above per SYNTAX.md */
static void check_fn_file(const SourceFile *sf, SemaResult *res)
{
  check_parse_errors(sf, res);
  if (sf->kind != FILE_FN)
    return;

  const Program *prog = &sf->program;
  const char *stem = sf->stem;
  int slen = (int) strlen(stem);

  if (prog->fn_count == 0)
  {
    push_error(res, "F001", sf->rel_path, 0, xsprintf("Expected a function named '%s'.", stem),
               xsprintf("Missing primary function in '%s.fn'", stem));
    return;
  }

  const FnDecl *primary = &prog->fns[prog->fn_count - 1];

  if (primary->name_len != slen || memcmp(primary->name, stem, (size_t) slen) != 0)
  {
    push_error(res, "F003", sf->rel_path, primary->line,
               xsprintf("Expected 'fn %s(...)' but found 'fn %.*s(...)'.", stem, primary->name_len,
                        primary->name),
               xsprintf("Function name mismatch in '%s.fn'", stem));
    return;
  }

  int match_count = 0, second_line = 0;
  for (int i = 0; i < prog->fn_count; i++)
  {
    const FnDecl *fn = &prog->fns[i];
    if (fn->name_len == slen && memcmp(fn->name, stem, (size_t) slen) == 0)
    {
      match_count++;
      if (match_count == 2)
        second_line = fn->line;
    }
  }
  if (match_count > 1)
  {
    push_error(res, "F002", sf->rel_path, second_line,
               xsprintf("Only one function named '%s' is allowed per .fn file.\n"
                        "  Move helpers above the primary function or into a .impl file.",
                        stem),
               xsprintf("Multiple public functions in '%s.fn'", stem));
  }
}

static int is_snake_case(const char *name)
{
  if (!name || !islower((unsigned char) name[0]))
    return 0;
  for (const char *p = name + 1; *p; p++)
    if (!islower((unsigned char) *p) && !isdigit((unsigned char) *p) && *p != '_')
      return 0;
  return 1;
}

/* A directory counts as a concept candidate only if it contains at least one
   .fn or .impl file — this excludes tool/doc dirs from generating C002. */
static int has_concept_files(const char *dir_path)
{
  DIR *d = opendir(dir_path);
  if (!d)
    return 0;
  struct dirent *ent;
  int found = 0;
  while (!found && (ent = readdir(d)) != NULL)
  {
    size_t nlen = strlen(ent->d_name);
    if ((nlen > 3 && strcmp(ent->d_name + nlen - 3, ".fn") == 0) ||
        (nlen > 5 && strcmp(ent->d_name + nlen - 5, ".impl") == 0))
      found = 1;
  }
  closedir(d);
  return found;
}

static void check_concept_directories(const Project *proj, SemaResult *res)
{
  /* C001 — declared concept has no directory */
  for (int i = 0; i < proj->concept_count; i++)
  {
    const Concept *c = &proj->concepts[i];
    if (strcmp(c->name, "main") == 0)
      continue;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", proj->root, c->name);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
      push_error(res, "C001", "project.meta", c->meta_line,
                 xsprintf("'%s' is declared in project.meta but no directory was found.", c->name),
                 xsprintf("Missing concept directory '%s'", c->name));
  }

  /* C002 — directory exists with concept files but is not declared */
  DIR *root_dir = opendir(proj->root);
  if (!root_dir)
    return;
  struct dirent *ent;
  while ((ent = readdir(root_dir)) != NULL)
  {
    const char *name = ent->d_name;
    if (name[0] == '.')
      continue;
    if (!is_snake_case(name))
      continue;
    if (project_concept_index(proj, name, (int) strlen(name)) >= 0)
      continue;

    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", proj->root, name);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
      continue;
    if (!has_concept_files(path))
      continue;

    char file_arg[512];
    snprintf(file_arg, sizeof(file_arg), "%s/", name);
    push_error(res, "C002", file_arg, 0,
               xsprintf("Directory '%s/' exists but is not declared in project.meta.", name),
               xsprintf("Undeclared concept '%s'", name));
  }
  closedir(root_dir);
}

/* Export set for a concept: .fn stems that actually declare the matching function */
typedef struct
{
  const char *name;
  int len;
} Export;

static Export *concept_exports(const Concept *c, int *out_count)
{
  Export *ex = NULL;
  int cap = 0;
  *out_count = 0;

  for (int i = 0; i < c->file_count; i++)
  {
    const SourceFile *sf = &c->files[i];
    if (sf->kind != FILE_FN)
      continue;

    const char *stem = sf->stem;
    int slen = (int) strlen(stem);
    int found = 0;
    for (int j = 0; j < sf->program.fn_count; j++)
    {
      const FnDecl *fn = &sf->program.fns[j];
      if (fn->name_len == slen && memcmp(fn->name, stem, (size_t) slen) == 0)
      {
        found = 1;
        break;
      }
    }
    if (!found)
      continue;

    if (*out_count == cap)
    {
      cap = cap ? cap * 2 : 4;
      ex = util_xrealloc(ex, (size_t) cap * sizeof(Export));
    }
    ex[(*out_count)++] = (Export){stem, slen};
  }
  return ex;
}

static void check_block_imports(const Block *block, int concept_idx, const char *rel_path,
                                const Project *proj, SemaResult *res);

static void check_qual_call(const Expr *expr, int concept_idx, const char *rel_path,
                            const Project *proj, SemaResult *res)
{
  if (!expr || expr->kind != EXPR_QUAL_CALL)
    return;

  const char *tgt_name = expr->as.qual_call.concept;
  int tgt_len = expr->as.qual_call.concept_len;
  const char *fn_name = expr->as.qual_call.name;
  int fn_len = expr->as.qual_call.name_len;
  int line = expr->line;

  int tgt_idx = project_concept_index(proj, tgt_name, tgt_len);
  if (tgt_idx < 0)
  {
    push_error(res, "I002", rel_path, line,
               xsprintf("No concept named '%.*s' exists in this project.", tgt_len, tgt_name),
               xsprintf("Unknown concept '%.*s'", tgt_len, tgt_name));
    return;
  }

  if (!project_has_edge(proj, concept_idx, tgt_idx))
  {
    push_error(res, "I001", rel_path, line,
               xsprintf("%s -> %.*s is not declared in project.meta.",
                        proj->concepts[concept_idx].name, tgt_len, tgt_name),
               xstrdup("Illegal cross-concept call"));
    return;
  }

  const Concept *tgt_concept = &proj->concepts[tgt_idx];
  int ex_count = 0;
  Export *ex = concept_exports(tgt_concept, &ex_count);

  int found = 0;
  for (int i = 0; i < ex_count; i++)
  {
    if (ex[i].len == fn_len && memcmp(ex[i].name, fn_name, (size_t) fn_len) == 0)
    {
      found = 1;
      break;
    }
  }

  if (!found)
  {
    /* Check if the function exists in a .impl file — that's I004, not I003 */
    int in_impl = 0;
    const char *impl_file = NULL;
    for (int i = 0; i < tgt_concept->file_count; i++)
    {
      const SourceFile *sf = &tgt_concept->files[i];
      if (sf->kind != FILE_IMPL)
        continue;
      for (int j = 0; j < sf->program.fn_count; j++)
      {
        const FnDecl *fn = &sf->program.fns[j];
        if (fn->name_len == fn_len && memcmp(fn->name, fn_name, (size_t) fn_len) == 0)
        {
          in_impl = 1;
          impl_file = sf->rel_path;
          break;
        }
      }
      if (in_impl)
        break;
    }

    if (in_impl)
    {
      push_error(res, "I004", rel_path, line,
                 xsprintf("'%.*s' is defined in %s and is not visible outside concept '%.*s'.",
                          fn_len, fn_name, impl_file, tgt_len, tgt_name),
                 xsprintf("Cannot access internal function '%.*s'", fn_len, fn_name));
    }
    else
    {
      int buf_sz = 64;
      char *buf = xmalloc((size_t) buf_sz);
      buf[0] = '\0';
      int pos = 0;
      for (int i = 0; i < ex_count; i++)
      {
        int needed = ex[i].len + (i > 0 ? 2 : 0) + 1;
        while (pos + needed >= buf_sz)
        {
          buf_sz *= 2;
          buf = util_xrealloc(buf, (size_t) buf_sz);
        }
        if (i > 0)
        {
          memcpy(buf + pos, ", ", 2);
          pos += 2;
        }
        memcpy(buf + pos, ex[i].name, (size_t) ex[i].len);
        pos += ex[i].len;
        buf[pos] = '\0';
      }
      if (ex_count == 0)
      {
        free(buf);
        buf = xstrdup("(none)");
      }

      push_error(res, "I003", rel_path, line,
                 xsprintf("'%.*s' is not exported by concept '%.*s'.\n"
                          "  Exported functions: %s",
                          fn_len, fn_name, tgt_len, tgt_name, buf),
                 xsprintf("Unknown function '%.*s::%.*s'", tgt_len, tgt_name, fn_len, fn_name));
      free(buf);
    }
  }

  free(ex);
}

static void check_expr_imports(const Expr *expr, int concept_idx, const char *rel_path,
                               const Project *proj, SemaResult *res)
{
  if (!expr)
    return;
  switch (expr->kind)
  {
    case EXPR_QUAL_CALL:
      check_qual_call(expr, concept_idx, rel_path, proj, res);
      for (int i = 0; i < expr->as.qual_call.args.count; i++)
        check_expr_imports(expr->as.qual_call.args.args[i], concept_idx, rel_path, proj, res);
      break;
    case EXPR_CALL:
      for (int i = 0; i < expr->as.call.args.count; i++)
        check_expr_imports(expr->as.call.args.args[i], concept_idx, rel_path, proj, res);
      break;
    case EXPR_UNARY:
      check_expr_imports(expr->as.unary.operand, concept_idx, rel_path, proj, res);
      break;
    case EXPR_BINARY:
      check_expr_imports(expr->as.binary.left, concept_idx, rel_path, proj, res);
      check_expr_imports(expr->as.binary.right, concept_idx, rel_path, proj, res);
      break;
    case EXPR_ASSIGN:
      check_expr_imports(expr->as.assign.value, concept_idx, rel_path, proj, res);
      break;
    default:
      break;
  }
}

static void check_block_imports(const Block *block, int concept_idx, const char *rel_path,
                                const Project *proj, SemaResult *res)
{
  for (int i = 0; i < block->count; i++)
  {
    const Stmt *s = block->stmts[i];
    if (!s)
      continue;
    switch (s->kind)
    {
      case STMT_LET:
        check_expr_imports(s->as.let.init, concept_idx, rel_path, proj, res);
        break;
      case STMT_CONST:
        check_expr_imports(s->as.konst.init, concept_idx, rel_path, proj, res);
        break;
      case STMT_RETURN:
        check_expr_imports(s->as.ret.value, concept_idx, rel_path, proj, res);
        break;
      case STMT_IF:
        check_expr_imports(s->as.sif.cond, concept_idx, rel_path, proj, res);
        check_block_imports(&s->as.sif.then_block, concept_idx, rel_path, proj, res);
        check_block_imports(&s->as.sif.else_block, concept_idx, rel_path, proj, res);
        break;
      case STMT_WHILE:
        check_expr_imports(s->as.swhile.cond, concept_idx, rel_path, proj, res);
        check_block_imports(&s->as.swhile.body, concept_idx, rel_path, proj, res);
        break;
      case STMT_LOOP:
        check_block_imports(&s->as.sloop.body, concept_idx, rel_path, proj, res);
        break;
      case STMT_EXPR:
        check_expr_imports(s->as.sexpr.expr, concept_idx, rel_path, proj, res);
        break;
      case STMT_BREAK:
      case STMT_CONTINUE:
        break;
    }
  }
}

static void check_file_imports(const SourceFile *sf, int concept_idx, const Project *proj,
                               SemaResult *res)
{
  for (int i = 0; i < sf->program.fn_count; i++)
  {
    const FnDecl *fn = &sf->program.fns[i];
    check_block_imports(&fn->body, concept_idx, sf->rel_path, proj, res);
  }
}

static void check_imports(const Project *proj, SemaResult *res)
{
  for (int i = 0; i < proj->concept_count; i++)
  {
    const Concept *c = &proj->concepts[i];
    for (int j = 0; j < c->file_count; j++)
      check_file_imports(&c->files[j], i, proj, res);
  }
}

/* -----------------------------------------------------------------------
 * Type checker  (T001, T003, T004, T005)
 *
 * T002 (unknown type) is already caught by the parser's parse_type.
 * --------------------------------------------------------------------- */

typedef struct
{
  const char *name;
  int name_len;
  TypeKind type;
  int is_mut;
} VarEntry;

typedef struct
{
  VarEntry *vars;
  int count;
  int cap;
} Scope;

static void scope_push(Scope *s, const char *name, int name_len, TypeKind type, int is_mut)
{
  if (s->count == s->cap)
  {
    s->cap = s->cap ? s->cap * 2 : 8;
    s->vars = util_xrealloc(s->vars, (size_t) s->cap * sizeof(VarEntry));
  }
  s->vars[s->count++] = (VarEntry){name, name_len, type, is_mut};
}

static VarEntry *scope_lookup(Scope *s, const char *name, int name_len)
{
  /* Walk backwards so inner-scope declarations shadow outer ones */
  for (int i = s->count - 1; i >= 0; i--)
  {
    if (s->vars[i].name_len == name_len && memcmp(s->vars[i].name, name, (size_t) name_len) == 0)
      return &s->vars[i];
  }
  return NULL;
}

static const char *type_str(TypeKind t)
{
  switch (t)
  {
    case TYPE_I8:
      return "i8";
    case TYPE_I16:
      return "i16";
    case TYPE_I32:
      return "i32";
    case TYPE_I64:
      return "i64";
    case TYPE_U8:
      return "u8";
    case TYPE_U16:
      return "u16";
    case TYPE_U32:
      return "u32";
    case TYPE_U64:
      return "u64";
    case TYPE_F32:
      return "f32";
    case TYPE_F64:
      return "f64";
    case TYPE_BOOL:
      return "bool";
    case TYPE_STR:
      return "str";
    case TYPE_PTR:
      return "ptr";
    case TYPE_VOID:
      return "void";
    default:
      return "?";
  }
}

static int is_numeric(TypeKind t)
{
  return t >= TYPE_I8 && t <= TYPE_F64;
}

static int is_integer(TypeKind t)
{
  return t >= TYPE_I8 && t <= TYPE_U64;
}

/* -----------------------------------------------------------------------
 * Function symbol table
 * --------------------------------------------------------------------- */

typedef struct
{
  int concept_idx;
  const char *name;
  int name_len;
  const Param *params;
  int param_count;
  TypeKind return_type;
  int is_variadic;
} FnSymbol;

typedef struct
{
  int concept_idx;
  const char *name;
  int name_len;
  TypeKind type;
} GlobalVarSymbol;

typedef struct
{
  FnSymbol *fn_syms;
  int fn_count;
  int fn_cap;
  GlobalVarSymbol *var_syms;
  int var_count;
  int var_cap;
  const Project *proj;
} SymTable;

static void symtable_push_fn(SymTable *t, FnSymbol s)
{
  if (t->fn_count == t->fn_cap)
  {
    t->fn_cap = t->fn_cap ? t->fn_cap * 2 : 32;
    t->fn_syms = util_xrealloc(t->fn_syms, (size_t) t->fn_cap * sizeof(FnSymbol));
  }
  t->fn_syms[t->fn_count++] = s;
}

static void symtable_push_var(SymTable *t, GlobalVarSymbol s)
{
  if (t->var_count == t->var_cap)
  {
    t->var_cap = t->var_cap ? t->var_cap * 2 : 32;
    t->var_syms = util_xrealloc(t->var_syms, (size_t) t->var_cap * sizeof(GlobalVarSymbol));
  }
  t->var_syms[t->var_count++] = s;
}

static void symtable_build(const Project *proj, SymTable *t)
{
  memset(t, 0, sizeof(*t));
  t->proj = proj;
  for (int i = 0; i < proj->concept_count; i++)
  {
    const Concept *c = &proj->concepts[i];
    for (int j = 0; j < c->file_count; j++)
    {
      const SourceFile *sf = &c->files[j];
      for (int k = 0; k < sf->program.fn_count; k++)
      {
        const FnDecl *fn = &sf->program.fns[k];
        FnSymbol s;
        s.concept_idx = i;
        s.name = fn->name;
        s.name_len = fn->name_len;
        s.params = fn->params;
        s.param_count = fn->param_count;
        s.return_type = fn->return_type;
        s.is_variadic = 0;
        symtable_push_fn(t, s);
      }
      /* Externs are callable/accessible within the concept like any local fn/var */
      for (int k = 0; k < sf->program.extern_count; k++)
      {
        const ExternDecl *ex = &sf->program.externs[k];
        if (ex->kind == EXTERN_VAR)
        {
          GlobalVarSymbol s;
          s.concept_idx = i;
          s.name = ex->name;
          s.name_len = ex->name_len;
          s.type = ex->return_type;
          symtable_push_var(t, s);
        }
        else
        {
          FnSymbol s;
          s.concept_idx = i;
          s.name = ex->name;
          s.name_len = ex->name_len;
          s.params = ex->params;
          s.param_count = ex->param_count;
          s.return_type = ex->return_type;
          s.is_variadic = ex->is_variadic;
          symtable_push_fn(t, s);
        }
      }
    }
  }
}

/* Look up an unqualified call within a concept (all .fn and .impl fns). */
static const FnSymbol *symtable_lookup_local_fn(const SymTable *t, int concept_idx,
                                                const char *name, int name_len)
{
  for (int i = 0; i < t->fn_count; i++)
  {
    if (t->fn_syms[i].concept_idx == concept_idx && t->fn_syms[i].name_len == name_len &&
        memcmp(t->fn_syms[i].name, name, (size_t) name_len) == 0)
      return &t->fn_syms[i];
  }
  return NULL;
}

static const GlobalVarSymbol *symtable_lookup_local_var(const SymTable *t, int concept_idx,
                                                        const char *name, int name_len)
{
  for (int i = 0; i < t->var_count; i++)
  {
    if (t->var_syms[i].concept_idx == concept_idx && t->var_syms[i].name_len == name_len &&
        memcmp(t->var_syms[i].name, name, (size_t) name_len) == 0)
      return &t->var_syms[i];
  }
  return NULL;
}

/* Look up a qualified call: concept::fn. Only matches the primary exported
   function (last fn in a .fn file matching the stem) — import checking has
   already verified the call is legal, so we just need the type. */
static const FnSymbol *symtable_lookup_qual_fn(const SymTable *t, int concept_idx, const char *name,
                                               int name_len)
{
  for (int i = 0; i < t->fn_count; i++)
  {
    if (t->fn_syms[i].concept_idx == concept_idx && t->fn_syms[i].name_len == name_len &&
        memcmp(t->fn_syms[i].name, name, (size_t) name_len) == 0)
      return &t->fn_syms[i];
  }
  return NULL;
}

static TypeKind infer_expr(const Expr *e, const FnDecl *fn, Scope *scope, const char *rel_path,
                           const SymTable *symt, int concept_idx, TypeKind hint, SemaResult *res);

static void check_call_args(const ArgList *args, const FnSymbol *sym, const FnDecl *fn,
                            Scope *scope, const char *rel_path, int line, const SymTable *symt,
                            int concept_idx, SemaResult *res)
{
  /* For variadic functions, require at least the fixed params */
  if (sym->is_variadic)
  {
    if (args->count < sym->param_count)
    {
      push_error(res, "T001", rel_path, line,
                 xsprintf("'%.*s' expects at least %d argument(s), got %d.", sym->name_len,
                          sym->name, sym->param_count, args->count),
                 xstrdup("Argument count mismatch"));
      return;
    }
    /* Type-check only the fixed params; variadic args are unchecked */
    for (int i = 0; i < sym->param_count; i++)
    {
      TypeKind arg_type = infer_expr(args->args[i], fn, scope, rel_path, symt, concept_idx,
                                     sym->params[i].type, res);
      if (arg_type != TYPE_UNKNOWN && arg_type != sym->params[i].type)
      {
        push_error(res, "T001", rel_path, line,
                   xsprintf("Argument %d of '%.*s': expected %s, found %s.", i + 1, sym->name_len,
                            sym->name, type_str(sym->params[i].type), type_str(arg_type)),
                   xstrdup("Type mismatch"));
      }
    }
    return;
  }

  if (args->count != sym->param_count)
  {
    push_error(res, "T001", rel_path, line,
               xsprintf("'%.*s' expects %d argument(s), got %d.", sym->name_len, sym->name,
                        sym->param_count, args->count),
               xstrdup("Argument count mismatch"));
    return;
  }
  for (int i = 0; i < args->count; i++)
  {
    TypeKind arg_type =
        infer_expr(args->args[i], fn, scope, rel_path, symt, concept_idx, sym->params[i].type, res);
    if (arg_type != TYPE_UNKNOWN && arg_type != sym->params[i].type)
    {
      push_error(res, "T001", rel_path, line,
                 xsprintf("Argument %d of '%.*s': expected %s, found %s.", i + 1, sym->name_len,
                          sym->name, type_str(sym->params[i].type), type_str(arg_type)),
                 xstrdup("Type mismatch"));
    }
  }
}

/* Infer the type of an expression. Returns TYPE_UNKNOWN on failure (error
   already emitted or uninferrable without full type info).
   hint: the expected type at the use site, or TYPE_UNKNOWN if not known.
   Integer literals coerce to any integer type when a hint is provided. */
static TypeKind infer_expr(const Expr *e, const FnDecl *fn, Scope *scope, const char *rel_path,
                           const SymTable *symt, int concept_idx, TypeKind hint, SemaResult *res)
{
  if (!e)
    return TYPE_UNKNOWN;

  switch (e->kind)
  {
    case EXPR_INT_LIT:
      /* Coerce to any integer type when the context tells us what's expected */
      if (hint != TYPE_UNKNOWN && is_integer(hint))
        return hint;
      return TYPE_I32;
    case EXPR_FLOAT_LIT:
      return TYPE_F64;
    case EXPR_STR_LIT:
      return TYPE_STR;
    case EXPR_BOOL_LIT:
      return TYPE_BOOL;

    case EXPR_IDENT:
    {
      /* Fix #2: use the dedicated ident field, not the sval alias */
      VarEntry *v = scope_lookup(scope, e->as.ident.ptr, e->as.ident.len);
      if (v)
        return v->type;
      const GlobalVarSymbol *gv =
          symtable_lookup_local_var(symt, concept_idx, e->as.ident.ptr, e->as.ident.len);
      if (gv)
        return gv->type;
      push_error(res, "I005", rel_path, e->line,
                 xsprintf("'%.*s' is not defined in this concept.", e->as.ident.len,
                          e->as.ident.ptr),
                 xsprintf("Unknown identifier '%.*s'", e->as.ident.len, e->as.ident.ptr));
      return TYPE_UNKNOWN;
    }

    case EXPR_ASSIGN:
    {
      VarEntry *v = scope_lookup(scope, e->as.assign.name, e->as.assign.name_len);
      if (!v)
        return TYPE_UNKNOWN;

      if (!v->is_mut)
      {
        push_error(res, "T005", rel_path, e->line,
                   xsprintf("'%.*s' was declared without 'mut'.\n"
                            "  Change to: let mut %.*s: %s = ...",
                            e->as.assign.name_len, e->as.assign.name, e->as.assign.name_len,
                            e->as.assign.name, type_str(v->type)),
                   xsprintf("Reassignment of immutable variable '%.*s'", e->as.assign.name_len,
                            e->as.assign.name));
        return v->type;
      }

      TypeKind rhs =
          infer_expr(e->as.assign.value, fn, scope, rel_path, symt, concept_idx, v->type, res);
      if (rhs != TYPE_UNKNOWN && rhs != v->type)
      {
        push_error(res, "T001", rel_path, e->line,
                   xsprintf("Expected %s, found %s.", type_str(v->type), type_str(rhs)),
                   xstrdup("Type mismatch"));
      }
      return v->type;
    }

    case EXPR_UNARY:
    {
      TypeKind operand = infer_expr(e->as.unary.operand, fn, scope, rel_path, symt, concept_idx,
                                    TYPE_UNKNOWN, res);
      if (operand == TYPE_UNKNOWN)
        return TYPE_UNKNOWN;
      if (e->as.unary.op == TOK_BANG)
      {
        if (operand != TYPE_BOOL)
        {
          push_error(res, "T001", rel_path, e->line,
                     xsprintf("Operator '!' requires bool, found %s.", type_str(operand)),
                     xstrdup("Type mismatch"));
          return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
      }
      if (e->as.unary.op == TOK_MINUS)
      {
        if (!is_numeric(operand))
        {
          push_error(res, "T001", rel_path, e->line,
                     xsprintf("Operator '-' requires numeric type, found %s.", type_str(operand)),
                     xstrdup("Type mismatch"));
          return TYPE_UNKNOWN;
        }
        return operand;
      }
      return TYPE_UNKNOWN;
    }

    case EXPR_BINARY:
    {
      TypeKind lhs =
          infer_expr(e->as.binary.left, fn, scope, rel_path, symt, concept_idx, TYPE_UNKNOWN, res);
      TypeKind rhs =
          infer_expr(e->as.binary.right, fn, scope, rel_path, symt, concept_idx, lhs, res);
      if (lhs == TYPE_UNKNOWN || rhs == TYPE_UNKNOWN)
        return TYPE_UNKNOWN;

      TokenKind op = e->as.binary.op;

      if (op == TOK_EQ || op == TOK_NEQ)
      {
        if (lhs != rhs)
        {
          push_error(res, "T001", rel_path, e->line,
                     xsprintf("Expected %s, found %s.", type_str(lhs), type_str(rhs)),
                     xstrdup("Type mismatch"));
          return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
      }
      if (op == TOK_LT || op == TOK_GT || op == TOK_LTE || op == TOK_GTE)
      {
        if (lhs != rhs || !is_numeric(lhs))
        {
          push_error(res, "T001", rel_path, e->line,
                     lhs != rhs
                         ? xsprintf("Expected %s, found %s.", type_str(lhs), type_str(rhs))
                         : xsprintf("Comparison requires numeric type, found %s.", type_str(lhs)),
                     xstrdup("Type mismatch"));
          return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
      }
      if (op == TOK_AND || op == TOK_OR)
      {
        if (lhs != TYPE_BOOL || rhs != TYPE_BOOL)
        {
          push_error(res, "T001", rel_path, e->line,
                     xsprintf("Operator '%s' requires bool operands.", op == TOK_AND ? "&&" : "||"),
                     xstrdup("Type mismatch"));
          return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
      }
      if (lhs != rhs)
      {
        push_error(res, "T001", rel_path, e->line,
                   xsprintf("Expected %s, found %s.", type_str(lhs), type_str(rhs)),
                   xstrdup("Type mismatch"));
        return TYPE_UNKNOWN;
      }
      if (!is_numeric(lhs))
      {
        push_error(res, "T001", rel_path, e->line,
                   xsprintf("Arithmetic requires numeric type, found %s.", type_str(lhs)),
                   xstrdup("Type mismatch"));
        return TYPE_UNKNOWN;
      }
      if (op == TOK_PERCENT && !is_integer(lhs))
      {
        push_error(res, "T001", rel_path, e->line,
                   xsprintf("Operator '%%' requires integer type, found %s.", type_str(lhs)),
                   xstrdup("Type mismatch"));
        return TYPE_UNKNOWN;
      }
      return lhs;
    }

    case EXPR_CALL:
    {
      const FnSymbol *sym =
          symtable_lookup_local_fn(symt, concept_idx, e->as.call.name, e->as.call.name_len);
      if (!sym)
      {
        push_error(res, "I005", rel_path, e->line,
                   xsprintf("'%.*s' is not defined in this concept.", e->as.call.name_len,
                            e->as.call.name),
                   xsprintf("Unknown function '%.*s'", e->as.call.name_len, e->as.call.name));
        return TYPE_UNKNOWN;
      }
      check_call_args(&e->as.call.args, sym, fn, scope, rel_path, e->line, symt, concept_idx, res);
      return sym->return_type;
    }

    case EXPR_QUAL_CALL:
    {
      int tgt_idx =
          project_concept_index(symt->proj, e->as.qual_call.concept, e->as.qual_call.concept_len);
      if (tgt_idx < 0)
        return TYPE_UNKNOWN;
      const FnSymbol *sym =
          symtable_lookup_qual_fn(symt, tgt_idx, e->as.qual_call.name, e->as.qual_call.name_len);
      if (!sym)
        return TYPE_UNKNOWN;
      check_call_args(&e->as.qual_call.args, sym, fn, scope, rel_path, e->line, symt, concept_idx,
                      res);
      return sym->return_type;
    }
  }

  return TYPE_UNKNOWN;
}

/* Returns 1 if an expression is a boolean literal 'true' */
static int expr_is_true_literal(const Expr *e)
{
  return e && e->kind == EXPR_BOOL_LIT && e->as.bval;
}

/* Returns 1 if all code paths in block return a value, 0 otherwise. */
static int block_always_returns(const Block *block)
{
  for (int i = 0; i < block->count; i++)
  {
    const Stmt *s = block->stmts[i];
    if (!s)
      continue;
    switch (s->kind)
    {
      case STMT_RETURN:
        return 1;
      case STMT_IF:
        /* Only certain if both branches exist and both always return */
        if (s->as.sif.else_block.count > 0 && block_always_returns(&s->as.sif.then_block) &&
            block_always_returns(&s->as.sif.else_block))
          return 1;
        break;
      case STMT_LOOP:
        /* An unconditional loop is a definite return if its body always
           returns — the only way out otherwise would be break, which
           doesn't satisfy a return requirement. */
        if (block_always_returns(&s->as.sloop.body))
          return 1;
        break;
      case STMT_WHILE:
        /* while true { ... } with a body that always returns is definite.
           A general while loop is not, because the condition may be false
           on the first iteration and the body never executes. */
        if (expr_is_true_literal(s->as.swhile.cond) && block_always_returns(&s->as.swhile.body))
          return 1;
        break;
      default:
        break;
    }
  }
  return 0;
}

static void check_block_types(const Block *block, const FnDecl *fn, Scope *scope,
                              const char *rel_path, const SymTable *symt, int concept_idx,
                              int loop_depth, SemaResult *res);

static void check_stmt_types(const Stmt *s, const FnDecl *fn, Scope *scope, const char *rel_path,
                             const SymTable *symt, int concept_idx, int loop_depth, SemaResult *res)
{
  if (!s)
    return;
  switch (s->kind)
  {

    case STMT_LET:
    {
      /* Pass declared type as hint so integer literals coerce correctly */
      TypeKind init_type =
          infer_expr(s->as.let.init, fn, scope, rel_path, symt, concept_idx, s->as.let.type, res);
      if (init_type != TYPE_UNKNOWN && init_type != s->as.let.type)
      {
        push_error(
            res, "T001", rel_path, s->line,
            xsprintf("Expected %s, found %s.", type_str(s->as.let.type), type_str(init_type)),
            xstrdup("Type mismatch"));
      }
      scope_push(scope, s->as.let.name, s->as.let.name_len, s->as.let.type, s->as.let.is_mut);
      break;
    }

    case STMT_CONST:
    {
      TypeKind init_type = infer_expr(s->as.konst.init, fn, scope, rel_path, symt, concept_idx,
                                      s->as.konst.type, res);
      if (init_type != TYPE_UNKNOWN && init_type != s->as.konst.type)
      {
        push_error(
            res, "T001", rel_path, s->line,
            xsprintf("Expected %s, found %s.", type_str(s->as.konst.type), type_str(init_type)),
            xstrdup("Type mismatch"));
      }
      scope_push(scope, s->as.konst.name, s->as.konst.name_len, s->as.konst.type, /*is_mut=*/0);
      break;
    }

    case STMT_RETURN:
    {
      if (fn->return_type == TYPE_VOID && s->as.ret.value)
      {
        push_error(res, "T004", rel_path, s->line,
                   xsprintf("Function '%.*s' has no return type but returns a value.", fn->name_len,
                            fn->name),
                   xsprintf("Unexpected return value in '%.*s'", fn->name_len, fn->name));
        break;
      }
      if (fn->return_type != TYPE_VOID && !s->as.ret.value)
      {
        push_error(res, "S005", rel_path, s->line,
                   xsprintf("Function '%.*s' declares return type %s but 'return' has no value.",
                            fn->name_len, fn->name, type_str(fn->return_type)),
                   xsprintf("Bare return in non-void function '%.*s'", fn->name_len, fn->name));
        break;
      }
      if (fn->return_type != TYPE_VOID && s->as.ret.value)
      {
        TypeKind ret_type = infer_expr(s->as.ret.value, fn, scope, rel_path, symt, concept_idx,
                                       fn->return_type, res);
        if (ret_type != TYPE_UNKNOWN && ret_type != fn->return_type)
        {
          push_error(
              res, "T001", rel_path, s->line,
              xsprintf("Expected %s, found %s.", type_str(fn->return_type), type_str(ret_type)),
              xstrdup("Type mismatch"));
        }
      }
      break;
    }

    case STMT_IF:
      infer_expr(s->as.sif.cond, fn, scope, rel_path, symt, concept_idx, TYPE_UNKNOWN, res);
      check_block_types(&s->as.sif.then_block, fn, scope, rel_path, symt, concept_idx, loop_depth,
                        res);
      check_block_types(&s->as.sif.else_block, fn, scope, rel_path, symt, concept_idx, loop_depth,
                        res);
      break;

    case STMT_WHILE:
    {
      TypeKind cond =
          infer_expr(s->as.swhile.cond, fn, scope, rel_path, symt, concept_idx, TYPE_UNKNOWN, res);
      if (cond != TYPE_UNKNOWN && cond != TYPE_BOOL)
      {
        push_error(res, "T001", rel_path, s->line,
                   xsprintf("While condition must be bool, found %s.", type_str(cond)),
                   xstrdup("Type mismatch"));
      }
      check_block_types(&s->as.swhile.body, fn, scope, rel_path, symt, concept_idx, loop_depth + 1,
                        res);
      break;
    }

    case STMT_LOOP:
      check_block_types(&s->as.sloop.body, fn, scope, rel_path, symt, concept_idx, loop_depth + 1,
                        res);
      break;

    case STMT_EXPR:
      infer_expr(s->as.sexpr.expr, fn, scope, rel_path, symt, concept_idx, TYPE_UNKNOWN, res);
      break;

    case STMT_BREAK:
    case STMT_CONTINUE:
      if (loop_depth == 0)
      {
        push_error(
            res, "S004", rel_path, s->line, NULL,
            xsprintf("'%s' used outside of a loop", s->kind == STMT_BREAK ? "break" : "continue"));
      }
      break;
  }
}

static void check_block_types(const Block *block, const FnDecl *fn, Scope *scope,
                              const char *rel_path, const SymTable *symt, int concept_idx,
                              int loop_depth, SemaResult *res)
{
  /* NOTE (#11): scope uses a flat array with a count-restore to simulate block
     scoping. Variables declared in one branch of an if/else are pushed into the
     same backing array; the count is restored after each branch so they're not
     visible to later statements. This works correctly for sequential code.
     A future improvement would use a proper linked-frame scope to give each
     block a fully isolated view. */
  int saved = scope->count;
  for (int i = 0; i < block->count; i++)
    check_stmt_types(block->stmts[i], fn, scope, rel_path, symt, concept_idx, loop_depth, res);
  scope->count = saved;
}

static void check_fn_types(const FnDecl *fn, const char *rel_path, const SymTable *symt,
                           int concept_idx, SemaResult *res)
{
  Scope scope = {0};
  for (int i = 0; i < fn->param_count; i++)
    scope_push(&scope, fn->params[i].name, fn->params[i].name_len, fn->params[i].type,
               /*is_mut=*/0);

  check_block_types(&fn->body, fn, &scope, rel_path, symt, concept_idx, 0, res);

  if (fn->return_type != TYPE_VOID && !block_always_returns(&fn->body))
  {
    push_error(res, "T003", rel_path, fn->line,
               xsprintf("Function '%.*s' declares return type %s but not all "
                        "code paths return a value.",
                        fn->name_len, fn->name, type_str(fn->return_type)),
               xsprintf("Missing return in '%.*s'", fn->name_len, fn->name));
  }

  free(scope.vars);
}

static void check_types(const Project *proj, SemaResult *res)
{
  SymTable symt;
  symtable_build(proj, &symt);

  for (int i = 0; i < proj->concept_count; i++)
  {
    const Concept *c = &proj->concepts[i];
    for (int j = 0; j < c->file_count; j++)
    {
      const SourceFile *sf = &c->files[j];
      for (int k = 0; k < sf->program.fn_count; k++)
        check_fn_types(&sf->program.fns[k], sf->rel_path, &symt, i, res);
    }
  }

  free(symt.fn_syms);
  free(symt.var_syms);
}

void sema_check(const Project *proj, SemaResult *res)
{
  memset(res, 0, sizeof(*res));
  check_entry_point(proj, res);
  check_concept_directories(proj, res);
  for (int i = 0; i < proj->concept_count; i++)
  {
    const Concept *c = &proj->concepts[i];
    for (int j = 0; j < c->file_count; j++)
      check_fn_file(&c->files[j], res);
  }
  check_imports(proj, res);
  check_types(proj, res);
}

/* All error output goes to stderr; the success message in main.c goes to stdout.
   This is intentional: errors can be separated from normal output by the shell. */
void sema_print(const SemaResult *result)
{
  for (int i = 0; i < result->count; i++)
  {
    const SemaError *e = &result->errors[i];
    if (i)
      fprintf(stderr, "\n");
    fprintf(stderr, "Error %s: %s\n", e->code, e->message);
    if (e->file && e->line)
      fprintf(stderr, "\n  --> %s:%d\n", e->file, e->line);
    else if (e->file)
      fprintf(stderr, "\n  --> %s\n", e->file);
    if (e->context)
      fprintf(stderr, "\n  %s\n", e->context);
  }
}

void sema_free(SemaResult *result)
{
  for (int i = 0; i < result->count; i++)
  {
    free(result->errors[i].message);
    free(result->errors[i].file);
    free(result->errors[i].context);
  }
  free(result->errors);
  memset(result, 0, sizeof(*result));
}
