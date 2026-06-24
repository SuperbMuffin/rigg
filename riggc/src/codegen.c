/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include "codegen.h"
#include "parser.h"
#include "project.h"
#include "util.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* helpers */

#define xmalloc util_xmalloc
#define xrealloc util_xrealloc
#define xstrdup util_xstrdup
#define xsprintf util_xsprintf

static void mkdir_p(const char *path)
{
  char tmp[4096];
  snprintf(tmp, sizeof(tmp), "%s", path);
  for (char *p = tmp + 1; *p; p++)
  {
    if (*p == '/')
    {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  mkdir(tmp, 0755);
}

/* IR type strings */

static const char *ir_type(TypeKind t)
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
      return "i8";
    case TYPE_U16:
      return "i16";
    case TYPE_U32:
      return "i32";
    case TYPE_U64:
      return "i64";
    case TYPE_F32:
      return "float";
    case TYPE_F64:
      return "double";
    case TYPE_BOOL:
      return "i1";
    case TYPE_STR:
      return "ptr";
    case TYPE_PTR:
      return "ptr";
    case TYPE_VOID:
      return "void";
    default:
      return "i32";
  }
}

static int type_is_float(TypeKind t)
{
  return t == TYPE_F32 || t == TYPE_F64;
}

static int type_is_unsigned(TypeKind t)
{
  return t == TYPE_U8 || t == TYPE_U16 || t == TYPE_U32 || t == TYPE_U64;
}

static int is_signed_int(TypeKind t)
{
  return t >= TYPE_I8 && t <= TYPE_I64;
}

static int is_repr_cast(TypeKind from, TypeKind to)
{
  return (from == TYPE_PTR && to == TYPE_STR) || (from == TYPE_STR && to == TYPE_PTR);
}

static int int_bit_width(TypeKind t)
{
  switch (t)
  {
    case TYPE_I8:
      return 8;
    case TYPE_I16:
      return 16;
    case TYPE_I32:
      return 32;
    case TYPE_I64:
      return 64;
    default:
      return 0;
  }
}

/* codegen context */

/* Interned string literal: escaped IR buffer + original source length */
typedef struct
{
  char *buf;       /* LLVM-escaped content, owned */
  int logical_len; /* number of characters after unescaping */
} StrLit;

/* One local variable slot: name → alloca register */
typedef struct
{
  char *name; /* owned */
  int name_len;
  int reg; /* alloca result: %N */
  TypeKind type;
} Local;

/* Loop frame for break/continue */
typedef struct
{
  int continue_label; /* continue target */
  int exit_label;     /* break target */
} LoopFrame;

#define MAX_LOOPS 64

typedef struct
{
  FILE *out;
  const Project *proj;
  int concept_idx;

  int reg;   /* next virtual register number */
  int label; /* next label number */

  Local *locals;
  int local_count;
  int local_cap;

  LoopFrame loop_stack[MAX_LOOPS];
  int loop_depth;

  /* string literals accumulated for the current function, emitted at top of
     module after all function bodies — actually we emit them at module top,
     so we collect them during the concept pass and emit before functions */
  StrLit *str_literals; /* IR constant strings, owned */
  int str_count;
  int str_cap;

  int bounds_check; /* emit runtime checks on ptr[index] when set */
  int terminated;   /* 1 if the current block has been terminated */
} CG;

static void cg_init(CG *cg, FILE *out, const Project *proj, int concept_idx, int bounds_check)
{
  memset(cg, 0, sizeof(*cg));
  cg->out = out;
  cg->proj = proj;
  cg->concept_idx = concept_idx;
  cg->reg = 0;
  cg->label = 0;
  cg->bounds_check = bounds_check;
}

static void cg_free(CG *cg)
{
  for (int i = 0; i < cg->local_count; i++)
    free(cg->locals[i].name);
  free(cg->locals);
  for (int i = 0; i < cg->str_count; i++)
    free(cg->str_literals[i].buf);
  free(cg->str_literals);
}

static void emit(CG *cg, const char *fmt, ...)
{
  if (cg->terminated && fmt[0] == ' ')
    return;
  if (fmt[0] == 'L')
    cg->terminated = 0;

  va_list ap;
  va_start(ap, fmt);
  vfprintf(cg->out, fmt, ap);
  va_end(ap);
}

/* Allocate a new virtual register number */
static int new_reg(CG *cg)
{
  return ++cg->reg;
}

/* Allocate a new label number */
static int new_label(CG *cg)
{
  return ++cg->label;
}

/* Push a local variable, return its alloca register */
static int push_local(CG *cg, const char *name, int name_len, TypeKind type, int alloca_reg)
{
  if (cg->local_count == cg->local_cap)
  {
    cg->local_cap = cg->local_cap ? cg->local_cap * 2 : 8;
    cg->locals = xrealloc(cg->locals, (size_t) cg->local_cap * sizeof(Local));
  }
  Local *l = &cg->locals[cg->local_count++];
  l->name = xmalloc((size_t) name_len + 1);
  memcpy(l->name, name, (size_t) name_len);
  l->name[name_len] = '\0';
  l->name_len = name_len;
  l->reg = alloca_reg;
  l->type = type;
  return alloca_reg;
}

/* Lookup a local by name, returns NULL if not found */
static const Local *find_local(const CG *cg, const char *name, int name_len)
{
  /* Walk backwards so inner-scope shadowing works if we ever allow it */
  for (int i = cg->local_count - 1; i >= 0; i--)
  {
    if (cg->locals[i].name_len == name_len &&
        memcmp(cg->locals[i].name, name, (size_t) name_len) == 0)
      return &cg->locals[i];
  }
  return NULL;
}

/* Save/restore local count for block scoping */
static int save_locals(const CG *cg)
{
  return cg->local_count;
}
static void restore_locals(CG *cg, int saved)
{
  for (int i = saved; i < cg->local_count; i++)
    free(cg->locals[i].name);
  cg->local_count = saved;
}

/* Intern a string literal; returns its global index (@.str.N).
   Converts source escape sequences to LLVM \XX hex escapes. */
static int intern_str(CG *cg, const char *ptr, int len)
{
  int idx = cg->str_count;
  if (cg->str_count == cg->str_cap)
  {
    cg->str_cap = cg->str_cap ? cg->str_cap * 2 : 8;
    cg->str_literals = xrealloc(cg->str_literals, (size_t) cg->str_cap * sizeof(StrLit));
  }
  /* Worst case: every char expands to \XX (3 bytes) */
  char *buf = xmalloc((size_t) len * 3 + 1);
  int out = 0;
  int logical_len = 0;
  for (int i = 0; i < len; i++)
  {
    logical_len++;
    if (ptr[i] == '\\' && i + 1 < len)
    {
      i++;
      switch (ptr[i])
      {
        case 'n':
          buf[out++] = '\\';
          buf[out++] = '0';
          buf[out++] = 'A';
          break;
        case 't':
          buf[out++] = '\\';
          buf[out++] = '0';
          buf[out++] = '9';
          break;
        case 'r':
          buf[out++] = '\\';
          buf[out++] = '0';
          buf[out++] = 'D';
          break;
        case '\\':
          buf[out++] = '\\';
          buf[out++] = '5';
          buf[out++] = 'C';
          break;
        case '"':
          buf[out++] = '\\';
          buf[out++] = '2';
          buf[out++] = '2';
          break;
        case '0':
          buf[out++] = '\\';
          buf[out++] = '0';
          buf[out++] = '0';
          break;
        default:
          buf[out++] = '\\';
          buf[out++] = ptr[i];
          break;
      }
    }
    else
    {
      buf[out++] = ptr[i];
    }
  }
  buf[out] = '\0';
  cg->str_literals[cg->str_count++] = (StrLit){buf, logical_len};
  return idx;
}

/* mangling */

/* concept__function — caller frees result */
static char *mangle(const char *concept, const char *fn, int fn_len)
{
  /* Special case: the 'main' function in the 'main' concept is the C entry
     point and must not be mangled. */
  if (strcmp(concept, "main") == 0 && fn_len == 4 && memcmp(fn, "main", 4) == 0)
    return xstrdup("main");
  size_t clen = strlen(concept);
  char *out = xmalloc(clen + 2 + (size_t) fn_len + 1);
  memcpy(out, concept, clen);
  out[clen] = '_';
  out[clen + 1] = '_';
  memcpy(out + clen + 2, fn, (size_t) fn_len);
  out[clen + 2 + fn_len] = '\0';
  return out;
}

/* forward declarations */

static int emit_expr(CG *cg, const Expr *e, TypeKind hint);
static int emit_ptr_index_addr(CG *cg, int ptr_reg, const Expr *index_expr);
static TypeKind infer_type(CG *cg, const Expr *e);
static void emit_block(CG *cg, const Block *block, const FnDecl *fn);
static void emit_stmt(CG *cg, const Stmt *s, const FnDecl *fn);

/* expression codegen */

/* Emit a call expression, return result register (0 for void).
   params/param_count: fixed parameter types (from FnDecl or ExternDecl).
   is_variadic: if set, args beyond param_count use default promotion types. */
static int emit_call(CG *cg, const char *mangled_name, TypeKind ret, const ArgList *args,
                     const Param *params, int param_count, int is_variadic)
{
  int arg_regs[64];
  TypeKind arg_types[64];
  int n = args->count < 64 ? args->count : 64;
  int fixed = param_count;
  (void) is_variadic;

  for (int i = 0; i < n; i++)
  {
    TypeKind hint = (i < fixed) ? params[i].type : TYPE_UNKNOWN;
    arg_regs[i] = emit_expr(cg, args->args[i], hint);
    /* For variadic args beyond fixed params, default to i32 for ints,
       double for floats — mirrors C's default argument promotions */
    if (i < fixed)
      arg_types[i] = params[i].type;
    else
    {
      arg_types[i] = infer_type(cg, args->args[i]);
      if (arg_types[i] == TYPE_UNKNOWN || arg_types[i] == TYPE_I32)
        arg_types[i] = TYPE_I32;
      else if (arg_types[i] == TYPE_F32)
        arg_types[i] = TYPE_F64;
    }
  }

  if (ret == TYPE_VOID)
    emit(cg, "  call void @%s(", mangled_name);
  else
  {
    int r = new_reg(cg);
    emit(cg, "  %%%d = call %s @%s(", r, ir_type(ret), mangled_name);
    for (int i = 0; i < n; i++)
    {
      if (i)
        emit(cg, ", ");
      emit(cg, "%s %%%d", ir_type(arg_types[i]), arg_regs[i]);
    }
    emit(cg, ")\n");
    return r;
  }
  for (int i = 0; i < n; i++)
  {
    if (i)
      emit(cg, ", ");
    emit(cg, "%s %%%d", ir_type(arg_types[i]), arg_regs[i]);
  }
  emit(cg, ")\n");
  return 0;
}

/* Find the FnDecl for a local call (within concept) */
static const FnDecl *find_fn_decl(const CG *cg, const char *name, int name_len)
{
  const Concept *c = &cg->proj->concepts[cg->concept_idx];
  for (int i = 0; i < c->file_count; i++)
  {
    const Program *p = &c->files[i].program;
    for (int j = 0; j < p->fn_count; j++)
    {
      const FnDecl *f = &p->fns[j];
      if (f->name_len == name_len && memcmp(f->name, name, (size_t) name_len) == 0)
        return f;
    }
  }
  return NULL;
}

/* Find an ExternDecl within the current concept */
static const ExternDecl *find_extern_decl(const CG *cg, const char *name, int name_len)
{
  const Concept *c = &cg->proj->concepts[cg->concept_idx];
  for (int i = 0; i < c->file_count; i++)
  {
    const Program *p = &c->files[i].program;
    for (int j = 0; j < p->extern_count; j++)
    {
      const ExternDecl *ex = &p->externs[j];
      if (ex->name_len == name_len && memcmp(ex->name, name, (size_t) name_len) == 0)
        return ex;
    }
  }
  return NULL;
}

/* Find the FnDecl for a qualified call (another concept) */
static const FnDecl *find_fn_decl_qual(const CG *cg, int tgt_concept_idx, const char *name,
                                       int name_len)
{
  const Concept *c = &cg->proj->concepts[tgt_concept_idx];
  for (int i = 0; i < c->file_count; i++)
  {
    const Program *p = &c->files[i].program;
    for (int j = 0; j < p->fn_count; j++)
    {
      const FnDecl *f = &p->fns[j];
      if (f->name_len == name_len && memcmp(f->name, name, (size_t) name_len) == 0)
        return f;
    }
  }
  return NULL;
}

static TypeKind infer_type(CG *cg, const Expr *e)
{
  if (!e)
    return TYPE_UNKNOWN;
  switch (e->kind)
  {
    case EXPR_INT_LIT:
      return TYPE_I32;
    case EXPR_FLOAT_LIT:
      return TYPE_F64;
    case EXPR_STR_LIT:
      return TYPE_STR;
    case EXPR_BOOL_LIT:
      return TYPE_BOOL;
    case EXPR_IDENT:
    {
      const Local *l = find_local(cg, e->as.ident.ptr, e->as.ident.len);
      if (l)
        return l->type;
      const ExternDecl *ex = find_extern_decl(cg, e->as.ident.ptr, e->as.ident.len);
      if (ex && ex->kind == EXTERN_VAR)
        return ex->return_type;
      return TYPE_UNKNOWN;
    }
    case EXPR_CALL:
    {
      const FnDecl *f = find_fn_decl(cg, e->as.call.name, e->as.call.name_len);
      if (f)
        return f->return_type;
      const ExternDecl *ex = find_extern_decl(cg, e->as.call.name, e->as.call.name_len);
      if (ex)
        return ex->return_type;
      return TYPE_UNKNOWN;
    }
    case EXPR_QUAL_CALL:
    {
      int tgt =
          project_concept_index(cg->proj, e->as.qual_call.concept, e->as.qual_call.concept_len);
      if (tgt < 0)
        return TYPE_UNKNOWN;
      const FnDecl *f = find_fn_decl_qual(cg, tgt, e->as.qual_call.name, e->as.qual_call.name_len);
      return f ? f->return_type : TYPE_UNKNOWN;
    }
    case EXPR_ASSIGN:
      return infer_type(cg, e->as.assign.value);
    case EXPR_UPDATE:
      return infer_type(cg, e->as.update.target);
    case EXPR_INDEX:
      return TYPE_I32;
    case EXPR_UNARY:
      return infer_type(cg, e->as.unary.operand);
    case EXPR_BINARY:
      return infer_type(cg, e->as.binary.left);
    case EXPR_CAST:
      return e->as.cast.target_type;
  }
  return TYPE_UNKNOWN;
}

static int emit_int_cast(CG *cg, int reg, TypeKind from, TypeKind to)
{
  if (from == to)
    return reg;
  int from_w = int_bit_width(from);
  int to_w = int_bit_width(to);
  int r = new_reg(cg);
  if (to_w > from_w)
    emit(cg, "  %%%d = sext %s %%%d to %s\n", r, ir_type(from), reg, ir_type(to));
  else
    emit(cg, "  %%%d = trunc %s %%%d to %s\n", r, ir_type(from), reg, ir_type(to));
  return r;
}

static const char *str_to_int_rt(TypeKind t)
{
  switch (t)
  {
    case TYPE_I8:
      return "rigg_str_to_i8";
    case TYPE_I16:
      return "rigg_str_to_i16";
    case TYPE_I32:
      return "rigg_str_to_i32";
    case TYPE_I64:
      return "rigg_str_to_i64";
    default:
      return NULL;
  }
}

static const char *int_to_str_rt(TypeKind t)
{
  switch (t)
  {
    case TYPE_I8:
      return "rigg_i8_to_str";
    case TYPE_I16:
      return "rigg_i16_to_str";
    case TYPE_I32:
      return "rigg_i32_to_str";
    case TYPE_I64:
      return "rigg_i64_to_str";
    default:
      return NULL;
  }
}

static int emit_cast(CG *cg, const Expr *e)
{
  TypeKind dst = e->as.cast.target_type;
  TypeKind src = infer_type(cg, e->as.cast.expr);
  int operand = emit_expr(cg, e->as.cast.expr, src != TYPE_UNKNOWN ? src : dst);

  if (is_repr_cast(src, dst))
    return operand;

  if (is_signed_int(src) && is_signed_int(dst))
    return emit_int_cast(cg, operand, src, dst);

  if (src == TYPE_STR && is_signed_int(dst))
  {
    const char *fn = str_to_int_rt(dst);
    int r = new_reg(cg);
    emit(cg, "  %%%d = call %s @%s(ptr %%%d)\n", r, ir_type(dst), fn, operand);
    return r;
  }

  if (is_signed_int(src) && dst == TYPE_STR)
  {
    const char *fn = int_to_str_rt(src);
    int r = new_reg(cg);
    emit(cg, "  %%%d = call ptr @%s(%s %%%d)\n", r, fn, ir_type(src), operand);
    return r;
  }

  if (src == TYPE_F32 && dst == TYPE_STR)
  {
    int r = new_reg(cg);
    emit(cg, "  %%%d = call ptr @rigg_f32_to_str(float %%%d)\n", r, operand);
    return r;
  }

  if (src == TYPE_F64 && dst == TYPE_STR)
  {
    int r = new_reg(cg);
    emit(cg, "  %%%d = call ptr @rigg_f64_to_str(double %%%d)\n", r, operand);
    return r;
  }

  if (src == TYPE_STR && dst == TYPE_F64)
  {
    int r = new_reg(cg);
    emit(cg, "  %%%d = call double @rigg_str_to_f64(ptr %%%d)\n", r, operand);
    return r;
  }
  if (src == TYPE_STR && dst == TYPE_F32)
  {
    int r = new_reg(cg);
    emit(cg, "  %%%d = call float @rigg_str_to_f32(ptr %%%d)\n", r, operand);
    return r;
  }

  if (src == TYPE_BOOL && dst == TYPE_STR)
  {
    int r = new_reg(cg);
    emit(cg, "  %%%d = call ptr @rigg_bool_to_str(i1 %%%d)\n", r, operand);
    return r;
  }

  if (src == TYPE_STR && dst == TYPE_BOOL)
  {
    int r = new_reg(cg);
    emit(cg, "  %%%d = call i1 @rigg_str_to_bool(ptr %%%d)\n", r, operand);
    return r;
  }

  /* integer -> ptr */
  if (is_signed_int(src) && dst == TYPE_PTR)
  {
    int r = new_reg(cg);
    emit(cg, "  %%%d = inttoptr %s %%%d to ptr\n", r, ir_type(src), operand);
    return r;
  }

  /* ptr -> integer */
  if (src == TYPE_PTR && is_signed_int(dst))
  {
    int r = new_reg(cg);
    emit(cg, "  %%%d = ptrtoint ptr %%%d to %s\n", r, operand, ir_type(dst));
    return r;
  }

  return operand;
}

/* Emit an expression; returns the register holding the result.
   For void expressions returns 0.
   hint: expected type at use site (used for integer literal width). */
static int emit_expr(CG *cg, const Expr *e, TypeKind hint)
{
  if (!e)
    return 0;

  switch (e->kind)
  {
    case EXPR_INT_LIT:
    {
      TypeKind t = (hint != TYPE_UNKNOWN && hint != TYPE_VOID) ? hint : TYPE_I32;
      int r = new_reg(cg);
      emit(cg, "  %%%d = add %s 0, %lld\n", r, ir_type(t), e->as.ival);
      return r;
    }

    case EXPR_FLOAT_LIT:
    {
      TypeKind t = (hint == TYPE_F32) ? TYPE_F32 : TYPE_F64;
      int r = new_reg(cg);
      emit(cg, "  %%%d = fadd %s 0.0, %g\n", r, ir_type(t), e->as.fval);
      return r;
    }

    case EXPR_BOOL_LIT:
    {
      int r = new_reg(cg);
      emit(cg, "  %%%d = add i1 0, %d\n", r, e->as.bval ? 1 : 0);
      return r;
    }

    case EXPR_STR_LIT:
    {
      int idx = intern_str(cg, e->as.sval.ptr, e->as.sval.len);
      int r = new_reg(cg);
      int arr_len = cg->str_literals[idx].logical_len + 1;
      emit(cg, "  %%%d = getelementptr inbounds [%d x i8], ptr @.str.%d, i32 0, i32 0\n", r,
           arr_len, idx);
      return r;
    }

    case EXPR_IDENT:
    {
      const Local *l = find_local(cg, e->as.ident.ptr, e->as.ident.len);
      if (l)
      {
        int r = new_reg(cg);
        emit(cg, "  %%%d = load %s, ptr %%%d\n", r, ir_type(l->type), l->reg);

        return r;
      }
      const ExternDecl *ex = find_extern_decl(cg, e->as.ident.ptr, e->as.ident.len);
      if (ex && ex->kind == EXTERN_VAR)
      {
        int r = new_reg(cg);
        emit(cg, "  %%%d = load %s, ptr @%.*s\n", r, ir_type(ex->return_type), ex->name_len,
             ex->name);
        return r;
      }
      return 0; /* sema already caught this */
    }

    case EXPR_ASSIGN:
    {
      Expr *target = e->as.assign.target;
      if (target->kind == EXPR_IDENT)
      {
        const Local *l = find_local(cg, target->as.ident.ptr, target->as.ident.len);
        if (!l)
          return 0;
        int val = emit_expr(cg, e->as.assign.value, l->type);
        emit(cg, "  store %s %%%d, ptr %%%d\n", ir_type(l->type), val, l->reg);
        return val;
      }
      if (target->kind == EXPR_INDEX)
      {
        int ptr = emit_expr(cg, target->as.index.target, TYPE_PTR);
        int addr = emit_ptr_index_addr(cg, ptr, target->as.index.index);
        int val = emit_expr(cg, e->as.assign.value, TYPE_I32);
        int byte = new_reg(cg);
        emit(cg, "  %%%d = trunc i32 %%%d to i8\n", byte, val);
        emit(cg, "  store i8 %%%d, ptr %%%d\n", byte, addr);
        return val;
      }
      return 0;
    }

    case EXPR_UPDATE:
    {
      Expr *target = e->as.update.target;
      if (target->kind != EXPR_IDENT)
        return 0;
      const Local *l = find_local(cg, target->as.ident.ptr, target->as.ident.len);
      if (!l)
        return 0;
      int cur = emit_expr(cg, target, TYPE_I32);
      int next = new_reg(cg);
      if (e->as.update.op == TOK_PLUS_PLUS)
        emit(cg, "  %%%d = add i32 %%%d, 1\n", next, cur);
      else
        emit(cg, "  %%%d = sub i32 %%%d, 1\n", next, cur);
      emit(cg, "  store i32 %%%d, ptr %%%d\n", next, l->reg);
      return next;
    }

    case EXPR_INDEX:
    {
      int ptr = emit_expr(cg, e->as.index.target, TYPE_PTR);
      int addr = emit_ptr_index_addr(cg, ptr, e->as.index.index);
      int byte = new_reg(cg);
      emit(cg, "  %%%d = load i8, ptr %%%d\n", byte, addr);
      int r = new_reg(cg);
      emit(cg, "  %%%d = zext i8 %%%d to i32\n", r, byte);
      return r;
    }

    case EXPR_UNARY:
    {
      int operand = emit_expr(cg, e->as.unary.operand, hint);
      int r = new_reg(cg);
      /* Infer operand type from hint or default to i32 */
      TypeKind t = (hint != TYPE_UNKNOWN && hint != TYPE_VOID) ? hint : TYPE_I32;
      if (e->as.unary.op == TOK_BANG)
      {
        emit(cg, "  %%%d = xor i1 %%%d, 1\n", r, operand);
      }
      else /* TOK_MINUS */
      {
        if (type_is_float(t))
          emit(cg, "  %%%d = fneg %s %%%d\n", r, ir_type(t), operand);
        else
          emit(cg, "  %%%d = sub %s 0, %%%d\n", r, ir_type(t), operand);
      }
      return r;
    }

    case EXPR_BINARY:
    {
      /* TODO: short-circuit evaluation for && and || */
      TokenKind op = e->as.binary.op;
      int is_cmp = op == TOK_EQ || op == TOK_NEQ || op == TOK_LT || op == TOK_GT || op == TOK_LTE ||
                   op == TOK_GTE;
      int is_logic = op == TOK_AND || op == TOK_OR;

      /* Comparisons yield i1 but compare operands at their own type; logical ops use i1. */
      TypeKind t;
      TypeKind emit_hint;
      if (is_cmp)
      {
        t = infer_type(cg, e->as.binary.left);
        if (t == TYPE_UNKNOWN)
          t = TYPE_I32;
        emit_hint = t;
      }
      else if (is_logic)
      {
        t = TYPE_BOOL;
        emit_hint = TYPE_BOOL;
      }
      else
      {
        t = infer_type(cg, e->as.binary.left);
        if (t == TYPE_UNKNOWN)
          t = (hint != TYPE_UNKNOWN && hint != TYPE_VOID && hint != TYPE_BOOL) ? hint : TYPE_I32;
        emit_hint = t;
      }

      int lhs = emit_expr(cg, e->as.binary.left, emit_hint);
      int rhs = emit_expr(cg, e->as.binary.right, emit_hint);
      int r = new_reg(cg);

      int is_fp = type_is_float(t);
      int is_uns = type_is_unsigned(t);

      if (is_fp)
      {
        switch (op)
        {
          case TOK_PLUS:
            emit(cg, "  %%%d = fadd %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_MINUS:
            emit(cg, "  %%%d = fsub %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_STAR:
            emit(cg, "  %%%d = fmul %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_SLASH:
            emit(cg, "  %%%d = fdiv %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_EQ:
            emit(cg, "  %%%d = fcmp oeq %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_NEQ:
            emit(cg, "  %%%d = fcmp one %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_LT:
            emit(cg, "  %%%d = fcmp olt %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_GT:
            emit(cg, "  %%%d = fcmp ogt %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_LTE:
            emit(cg, "  %%%d = fcmp ole %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_GTE:
            emit(cg, "  %%%d = fcmp oge %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          default:
            break;
        }
      }
      else
      {
        switch (op)
        {
          case TOK_PLUS:
            emit(cg, "  %%%d = add %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_MINUS:
            emit(cg, "  %%%d = sub %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_STAR:
            emit(cg, "  %%%d = mul %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_SLASH:
            if (is_uns)
              emit(cg, "  %%%d = udiv %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            else
              emit(cg, "  %%%d = sdiv %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_PERCENT:
            if (is_uns)
              emit(cg, "  %%%d = urem %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            else
              emit(cg, "  %%%d = srem %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_EQ:
            if (t == TYPE_STR)
              emit(cg, "  %%%d = call i1 @rigg_str_eq(ptr %%%d, ptr %%%d)\n", r, lhs, rhs);
            else
              emit(cg, "  %%%d = icmp eq  %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_NEQ:
            if (t == TYPE_STR)
              emit(cg, "  %%%d = call i1 @rigg_str_ne(ptr %%%d, ptr %%%d)\n", r, lhs, rhs);
            else
              emit(cg, "  %%%d = icmp ne  %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_LT:
            if (is_uns)
              emit(cg, "  %%%d = icmp ult %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            else
              emit(cg, "  %%%d = icmp slt %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_GT:
            if (is_uns)
              emit(cg, "  %%%d = icmp ugt %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            else
              emit(cg, "  %%%d = icmp sgt %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_LTE:
            if (is_uns)
              emit(cg, "  %%%d = icmp ule %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            else
              emit(cg, "  %%%d = icmp sle %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_GTE:
            if (is_uns)
              emit(cg, "  %%%d = icmp uge %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            else
              emit(cg, "  %%%d = icmp sge %s %%%d, %%%d\n", r, ir_type(t), lhs, rhs);
            break;
          case TOK_AND:
            emit(cg, "  %%%d = and i1 %%%d, %%%d\n", r, lhs, rhs);
            break;
          case TOK_OR:
            emit(cg, "  %%%d = or  i1 %%%d, %%%d\n", r, lhs, rhs);
            break;
          default:
            break;
        }
      }
      return r;
    }

    case EXPR_CALL:
    {
      const FnDecl *decl = find_fn_decl(cg, e->as.call.name, e->as.call.name_len);
      const ExternDecl *ext = NULL;
      if (!decl)
        ext = find_extern_decl(cg, e->as.call.name, e->as.call.name_len);
      TypeKind ret = decl ? decl->return_type : (ext ? ext->return_type : TYPE_VOID);
      int is_variadic = ext ? ext->is_variadic : 0;
      const char *cname = cg->proj->concepts[cg->concept_idx].name;
      char *mangled = mangle(cname, e->as.call.name, e->as.call.name_len);
      /* Extern functions use their real name, not mangled */
      if (ext)
      {
        free(mangled);
        mangled = xmalloc((size_t) e->as.call.name_len + 1);
        memcpy(mangled, e->as.call.name, (size_t) e->as.call.name_len);
        mangled[e->as.call.name_len] = '\0';
      }
      const Param *params = decl ? decl->params : (ext ? ext->params : NULL);
      int param_count = decl ? decl->param_count : (ext ? ext->param_count : 0);
      int r = emit_call(cg, mangled, ret, &e->as.call.args, params, param_count, is_variadic);
      free(mangled);
      return r;
    }

    case EXPR_QUAL_CALL:
    {
      int tgt =
          project_concept_index(cg->proj, e->as.qual_call.concept, e->as.qual_call.concept_len);
      const FnDecl *decl =
          tgt >= 0 ? find_fn_decl_qual(cg, tgt, e->as.qual_call.name, e->as.qual_call.name_len)
                   : NULL;
      TypeKind ret = decl ? decl->return_type : TYPE_VOID;
      const char *cname = tgt >= 0 ? cg->proj->concepts[tgt].name : "";
      char *mangled = mangle(cname, e->as.qual_call.name, e->as.qual_call.name_len);
      int r = emit_call(cg, mangled, ret, &e->as.qual_call.args, decl ? decl->params : NULL,
                        decl ? decl->param_count : 0,
                        /*is_variadic=*/0);
      free(mangled);
      return r;
    }

    case EXPR_CAST:
      return emit_cast(cg, e);
  }

  return 0;
}

static int emit_as_i32(CG *cg, const Expr *index_expr)
{
  TypeKind t = infer_type(cg, index_expr);
  int reg = emit_expr(cg, index_expr, TYPE_I32);
  if (t == TYPE_UNKNOWN || t == TYPE_I32)
    return reg;
  int r = new_reg(cg);
  emit(cg, "  %%%d = trunc %s %%%d to i32\n", r, ir_type(t), reg);
  return r;
}

static void emit_index_bounds_check(CG *cg, int index_reg)
{
  if (!cg->bounds_check)
    return;
  int ok = new_reg(cg);
  emit(cg, "  %%%d = icmp sge i32 %%%d, 0\n", ok, index_reg);
  int fail = new_label(cg);
  int cont = new_label(cg);
  emit(cg, "  br i1 %%%d, label %%L%d, label %%L%d\n", ok, cont, fail);
  emit(cg, "L%d:\n", fail);
  emit(cg, "  call void @abort()\n");
  emit(cg, "  unreachable\n");
  emit(cg, "L%d:\n", cont);
}

static int emit_ptr_index_addr(CG *cg, int ptr_reg, const Expr *index_expr)
{
  int index_reg = emit_as_i32(cg, index_expr);
  emit_index_bounds_check(cg, index_reg);
  int addr = new_reg(cg);
  emit(cg, "  %%%d = getelementptr i8, ptr %%%d, i32 %%%d\n", addr, ptr_reg, index_reg);
  return addr;
}

/* statement codegen */

static void emit_stmt(CG *cg, const Stmt *s, const FnDecl *fn)
{
  if (!s)
    return;

  switch (s->kind)
  {
    case STMT_LET:
    case STMT_CONST:
    {
      const char *name = s->kind == STMT_LET ? s->as.let.name : s->as.konst.name;
      int nlen = s->kind == STMT_LET ? s->as.let.name_len : s->as.konst.name_len;
      TypeKind type = s->kind == STMT_LET ? s->as.let.type : s->as.konst.type;
      const Expr *init = s->kind == STMT_LET ? s->as.let.init : s->as.konst.init;

      int alloca_reg = new_reg(cg);
      emit(cg, "  %%%d = alloca %s\n", alloca_reg, ir_type(type));

      if (init)
      {
        /* Evaluate the initializer BEFORE pushing the new local so that
           shadowed names (e.g. `let x = f(x)`) still resolve to the
           previous binding rather than the uninitialised new slot. */
        int val = emit_expr(cg, init, type);
        emit(cg, "  store %s %%%d, ptr %%%d\n", ir_type(type), val, alloca_reg);
      }

      push_local(cg, name, nlen, type, alloca_reg);
      break;
    }

    case STMT_RETURN:
    {
      if (!s->as.ret.value || fn->return_type == TYPE_VOID)
      {
        emit(cg, "  ret void\n");
      }
      else
      {
        int val = emit_expr(cg, s->as.ret.value, fn->return_type);
        emit(cg, "  ret %s %%%d\n", ir_type(fn->return_type), val);
      }
      cg->terminated = 1;
      break;
    }

    case STMT_IF:
    {
      int then_label = new_label(cg);
      int else_label = new_label(cg);
      int end_label = new_label(cg);
      int has_else = s->as.sif.else_block.count > 0;

      int cond = emit_expr(cg, s->as.sif.cond, TYPE_BOOL);
      emit(cg, "  br i1 %%%d, label %%L%d, label %%L%d\n", cond, then_label,
           has_else ? else_label : end_label);

      emit(cg, "L%d:\n", then_label);
      int saved = save_locals(cg);
      emit_block(cg, &s->as.sif.then_block, fn);
      restore_locals(cg, saved);
      emit(cg, "  br label %%L%d\n", end_label);

      if (has_else)
      {
        emit(cg, "L%d:\n", else_label);
        saved = save_locals(cg);
        emit_block(cg, &s->as.sif.else_block, fn);
        restore_locals(cg, saved);
        emit(cg, "  br label %%L%d\n", end_label);
      }

      emit(cg, "L%d:\n", end_label);
      break;
    }

    case STMT_FOR:
    {
      int header_label = new_label(cg);
      int body_label = new_label(cg);
      int post_label = new_label(cg);
      int exit_label = new_label(cg);

      int saved = save_locals(cg);
      emit_stmt(cg, s->as.sfor.init, fn);
      emit(cg, "  br label %%L%d\n", header_label);

      emit(cg, "L%d:\n", header_label);
      int cond = emit_expr(cg, s->as.sfor.cond, TYPE_BOOL);
      emit(cg, "  br i1 %%%d, label %%L%d, label %%L%d\n", cond, body_label, exit_label);

      emit(cg, "L%d:\n", body_label);
      cg->loop_stack[cg->loop_depth++] = (LoopFrame){post_label, exit_label};
      int body_saved = save_locals(cg);
      emit_block(cg, &s->as.sfor.body, fn);
      restore_locals(cg, body_saved);
      cg->loop_depth--;
      emit(cg, "  br label %%L%d\n", post_label);

      emit(cg, "L%d:\n", post_label);
      emit_expr(cg, s->as.sfor.post, TYPE_UNKNOWN);
      emit(cg, "  br label %%L%d\n", header_label);

      restore_locals(cg, saved);
      emit(cg, "L%d:\n", exit_label);
      break;
    }

    case STMT_WHILE:
    {
      int header_label = new_label(cg);
      int body_label = new_label(cg);
      int exit_label = new_label(cg);

      emit(cg, "  br label %%L%d\n", header_label);
      emit(cg, "L%d:\n", header_label);
      int cond = emit_expr(cg, s->as.swhile.cond, TYPE_BOOL);
      emit(cg, "  br i1 %%%d, label %%L%d, label %%L%d\n", cond, body_label, exit_label);

      emit(cg, "L%d:\n", body_label);
      cg->loop_stack[cg->loop_depth++] = (LoopFrame){header_label, exit_label};
      int saved = save_locals(cg);
      emit_block(cg, &s->as.swhile.body, fn);
      restore_locals(cg, saved);
      cg->loop_depth--;
      emit(cg, "  br label %%L%d\n", header_label);

      emit(cg, "L%d:\n", exit_label);
      break;
    }

    case STMT_LOOP:
    {
      int header_label = new_label(cg);
      int exit_label = new_label(cg);

      emit(cg, "  br label %%L%d\n", header_label);
      emit(cg, "L%d:\n", header_label);

      cg->loop_stack[cg->loop_depth++] = (LoopFrame){header_label, exit_label};
      int saved = save_locals(cg);
      emit_block(cg, &s->as.sloop.body, fn);
      restore_locals(cg, saved);
      cg->loop_depth--;
      emit(cg, "  br label %%L%d\n", header_label);

      emit(cg, "L%d:\n", exit_label);
      break;
    }

    case STMT_BREAK:
    {
      int exit = cg->loop_stack[cg->loop_depth - 1].exit_label;
      emit(cg, "  br label %%L%d\n", exit);
      /* Emit a dead block label so subsequent IR remains well-formed */
      emit(cg, "L%d:\n", new_label(cg));
      break;
    }

    case STMT_CONTINUE:
    {
      int cont = cg->loop_stack[cg->loop_depth - 1].continue_label;
      emit(cg, "  br label %%L%d\n", cont);
      emit(cg, "L%d:\n", new_label(cg));
      break;
    }

    case STMT_EXPR:
      emit_expr(cg, s->as.sexpr.expr, TYPE_UNKNOWN);
      break;
  }
}

static void emit_block(CG *cg, const Block *block, const FnDecl *fn)
{
  for (int i = 0; i < block->count; i++)
    emit_stmt(cg, block->stmts[i], fn);
}

/* function codegen */

static void emit_fn(CG *cg, const FnDecl *fn, const char *concept_name)
{
  /* Reset per-function state */
  cg->reg = 0;
  cg->label = 0;
  cg->loop_depth = 0;
  cg->terminated = 0;
  for (int i = 0; i < cg->local_count; i++)
    free(cg->locals[i].name);
  cg->local_count = 0;

  char *mangled = mangle(concept_name, fn->name, fn->name_len);

  /* The C runtime requires main() -> i32 regardless of what the Rigg source
     declares. A void main() in Rigg gets an implicit return 0. */
  int is_entry = strcmp(mangled, "main") == 0;
  TypeKind ir_ret = (is_entry && fn->return_type == TYPE_VOID) ? TYPE_I32 : fn->return_type;

  /* Function signature */
  emit(cg, "define %s @%s(", ir_type(ir_ret), mangled);
  for (int i = 0; i < fn->param_count; i++)
  {
    if (i)
      emit(cg, ", ");
    emit(cg, "%s %%p%d", ir_type(fn->params[i].type), i);
  }
  emit(cg, ") {\n");
  emit(cg, "entry:\n");

  /* Allocate parameters into locals so they're mutable like any other var */
  for (int i = 0; i < fn->param_count; i++)
  {
    int alloca_reg = new_reg(cg);
    emit(cg, "  %%%d = alloca %s\n", alloca_reg, ir_type(fn->params[i].type));
    emit(cg, "  store %s %%p%d, ptr %%%d\n", ir_type(fn->params[i].type), i, alloca_reg);
    push_local(cg, fn->params[i].name, fn->params[i].name_len, fn->params[i].type, alloca_reg);
  }

  emit_block(cg, &fn->body, fn);

  /* Implicit return at end of function body.
     For non-void functions sema guarantees all paths return explicitly, so
     this only fires for void functions (or unreachable tails).
     Entry point gets ret i32 0; all other void functions get ret void. */
  if (fn->return_type == TYPE_VOID)
  {
    if (is_entry)
      emit(cg, "  ret i32 0\n");
    else
      emit(cg, "  ret void\n");
  }

  emit(cg, "}\n\n");
  free(mangled);
}

/* declare externals for cross-concept calls */

/* Walk all expressions in a program and collect qualified call targets that
   live in other concepts — emit `declare` stubs for them. */

typedef struct
{
  char *mangled;
  TypeKind ret;
} Decl;

static void collect_decls_expr(const Expr *e, const Project *proj, int concept_idx, Decl **decls,
                               int *count, int *cap)
{
  if (!e)
    return;
  switch (e->kind)
  {
    case EXPR_QUAL_CALL:
    {
      int tgt = project_concept_index(proj, e->as.qual_call.concept, e->as.qual_call.concept_len);
      if (tgt < 0 || tgt == concept_idx)
        break;

      /* Find return type from the target concept */
      TypeKind ret = TYPE_VOID;
      const Concept *c = &proj->concepts[tgt];
      for (int i = 0; i < c->file_count; i++)
        for (int j = 0; j < c->files[i].program.fn_count; j++)
        {
          const FnDecl *f = &c->files[i].program.fns[j];
          if (f->name_len == e->as.qual_call.name_len &&
              memcmp(f->name, e->as.qual_call.name, (size_t) f->name_len) == 0)
          {
            ret = f->return_type;
            goto found;
          }
        }
    found:;
      char *m = mangle(proj->concepts[tgt].name, e->as.qual_call.name, e->as.qual_call.name_len);
      /* Deduplicate */
      for (int i = 0; i < *count; i++)
        if (strcmp((*decls)[i].mangled, m) == 0)
        {
          free(m);
          goto skip_args;
        }

      if (*count == *cap)
      {
        *cap = *cap ? *cap * 2 : 8;
        *decls = xrealloc(*decls, (size_t) *cap * sizeof(Decl));
      }
      (*decls)[(*count)++] = (Decl){m, ret};
    skip_args:
      for (int i = 0; i < e->as.qual_call.args.count; i++)
        collect_decls_expr(e->as.qual_call.args.args[i], proj, concept_idx, decls, count, cap);
      break;
    }
    case EXPR_CALL:
      for (int i = 0; i < e->as.call.args.count; i++)
        collect_decls_expr(e->as.call.args.args[i], proj, concept_idx, decls, count, cap);
      break;
    case EXPR_UNARY:
      collect_decls_expr(e->as.unary.operand, proj, concept_idx, decls, count, cap);
      break;
    case EXPR_BINARY:
      collect_decls_expr(e->as.binary.left, proj, concept_idx, decls, count, cap);
      collect_decls_expr(e->as.binary.right, proj, concept_idx, decls, count, cap);
      break;
    case EXPR_ASSIGN:
      collect_decls_expr(e->as.assign.target, proj, concept_idx, decls, count, cap);
      collect_decls_expr(e->as.assign.value, proj, concept_idx, decls, count, cap);
      break;
    case EXPR_UPDATE:
      collect_decls_expr(e->as.update.target, proj, concept_idx, decls, count, cap);
      break;
    case EXPR_INDEX:
      collect_decls_expr(e->as.index.target, proj, concept_idx, decls, count, cap);
      collect_decls_expr(e->as.index.index, proj, concept_idx, decls, count, cap);
      break;
    case EXPR_CAST:
      collect_decls_expr(e->as.cast.expr, proj, concept_idx, decls, count, cap);
      break;
    default:
      break;
  }
}

static void collect_decls_block(const Block *b, const Project *proj, int concept_idx, Decl **decls,
                                int *count, int *cap);

static void collect_decls_stmt(const Stmt *s, const Project *proj, int concept_idx, Decl **decls,
                               int *count, int *cap)
{
  if (!s)
    return;
  switch (s->kind)
  {
    case STMT_LET:
      collect_decls_expr(s->as.let.init, proj, concept_idx, decls, count, cap);
      break;
    case STMT_CONST:
      collect_decls_expr(s->as.konst.init, proj, concept_idx, decls, count, cap);
      break;
    case STMT_RETURN:
      collect_decls_expr(s->as.ret.value, proj, concept_idx, decls, count, cap);
      break;
    case STMT_EXPR:
      collect_decls_expr(s->as.sexpr.expr, proj, concept_idx, decls, count, cap);
      break;
    case STMT_IF:
      collect_decls_expr(s->as.sif.cond, proj, concept_idx, decls, count, cap);
      collect_decls_block(&s->as.sif.then_block, proj, concept_idx, decls, count, cap);
      collect_decls_block(&s->as.sif.else_block, proj, concept_idx, decls, count, cap);
      break;
    case STMT_FOR:
      collect_decls_stmt(s->as.sfor.init, proj, concept_idx, decls, count, cap);
      collect_decls_expr(s->as.sfor.cond, proj, concept_idx, decls, count, cap);
      collect_decls_expr(s->as.sfor.post, proj, concept_idx, decls, count, cap);
      collect_decls_block(&s->as.sfor.body, proj, concept_idx, decls, count, cap);
      break;
    case STMT_WHILE:
      collect_decls_expr(s->as.swhile.cond, proj, concept_idx, decls, count, cap);
      collect_decls_block(&s->as.swhile.body, proj, concept_idx, decls, count, cap);
      break;
    case STMT_LOOP:
      collect_decls_block(&s->as.sloop.body, proj, concept_idx, decls, count, cap);
      break;
    default:
      break;
  }
}

static void collect_decls_block(const Block *b, const Project *proj, int concept_idx, Decl **decls,
                                int *count, int *cap)
{
  for (int i = 0; i < b->count; i++)
    collect_decls_stmt(b->stmts[i], proj, concept_idx, decls, count, cap);
}

/* concept → .ll file */

static int emit_concept(const Project *proj, int concept_idx, const char *out_path,
                        const char *target_triple, int bounds_check)
{
  FILE *f = fopen(out_path, "w");
  if (!f)
  {
    fprintf(stderr, "rigg: cannot open '%s' for writing: %s\n", out_path, strerror(errno));
    return -1;
  }

  const Concept *c = &proj->concepts[concept_idx];

  /* Module header */
  fprintf(f, "; concept %s\n", c->name);
  fprintf(f, "source_filename = \"%s\"\n", c->name);
  if (target_triple && target_triple[0])
    fprintf(f, "target triple = \"%s\"\n", target_triple);
  fprintf(f, "\n");

  /* Collect all string literals across all files in the concept first */
  CG cg;
  cg_init(&cg, f, proj, concept_idx, bounds_check);

  if (bounds_check)
    fprintf(f, "declare void @abort()\n\n");

  fprintf(f, "declare i8 @rigg_str_to_i8(ptr)\n");
  fprintf(f, "declare i16 @rigg_str_to_i16(ptr)\n");
  fprintf(f, "declare i32 @rigg_str_to_i32(ptr)\n");
  fprintf(f, "declare i64 @rigg_str_to_i64(ptr)\n");
  fprintf(f, "declare float @rigg_str_to_f32(ptr)\n");
  fprintf(f, "declare double @rigg_str_to_f64(ptr)\n");
  fprintf(f, "declare ptr @rigg_i8_to_str(i8)\n");
  fprintf(f, "declare ptr @rigg_i16_to_str(i16)\n");
  fprintf(f, "declare ptr @rigg_i32_to_str(i32)\n");
  fprintf(f, "declare ptr @rigg_i64_to_str(i64)\n");
  fprintf(f, "declare ptr @rigg_f32_to_str(float)\n");
  fprintf(f, "declare ptr @rigg_f64_to_str(double)\n");
  fprintf(f, "declare ptr @rigg_bool_to_str(i1)\n");
  fprintf(f, "declare i1 @rigg_str_to_bool(ptr)\n");
  fprintf(f, "declare i1 @rigg_str_eq(ptr, ptr)\n");
  fprintf(f, "declare i1 @rigg_str_ne(ptr, ptr)\n\n");

  /* Collect external declares needed by this concept (cross-concept calls) */
  Decl *decls = NULL;
  int dcnt = 0, dcap = 0;
  for (int i = 0; i < c->file_count; i++)
  {
    const Program *prog = &c->files[i].program;
    for (int j = 0; j < prog->fn_count; j++)
      collect_decls_block(&prog->fns[j].body, proj, concept_idx, &decls, &dcnt, &dcap);
  }

  /* Emit declare stubs for cross-concept calls */
  for (int i = 0; i < dcnt; i++)
  {
    fprintf(f, "declare %s @%s(...)\n", ir_type(decls[i].ret), decls[i].mangled);
    free(decls[i].mangled);
  }
  free(decls);
  if (dcnt)
    fprintf(f, "\n");

  /* Emit declare stubs for extern declarations */
  for (int i = 0; i < c->file_count; i++)
  {
    const Program *prog = &c->files[i].program;
    for (int j = 0; j < prog->extern_count; j++)
    {
      const ExternDecl *ex = &prog->externs[j];
      if (ex->kind == EXTERN_VAR)
      {
        fprintf(f, "@%.*s = external global %s\n", ex->name_len, ex->name,
                ir_type(ex->return_type));
      }
      else
      {
        fprintf(f, "declare %s @%.*s(", ir_type(ex->return_type), ex->name_len, ex->name);
        for (int k = 0; k < ex->param_count; k++)
        {
          if (k)
            fprintf(f, ", ");
          fprintf(f, "%s", ir_type(ex->params[k].type));
        }
        if (ex->is_variadic)
        {
          if (ex->param_count)
            fprintf(f, ", ");
          fprintf(f, "...");
        }
        fprintf(f, ")\n");
      }
    }
  }
  if (c->file_count)
    fprintf(f, "\n");

  /* Emit all functions; string literals are accumulated in cg */
  for (int i = 0; i < c->file_count; i++)
  {
    const Program *prog = &c->files[i].program;
    for (int j = 0; j < prog->fn_count; j++)
      emit_fn(&cg, &prog->fns[j], c->name);
  }

  /* Emit string literal globals at end of file */
  for (int i = 0; i < cg.str_count; i++)
  {
    const StrLit *sl = &cg.str_literals[i];
    fprintf(f, "@.str.%d = private unnamed_addr constant [%d x i8] c\"%s\\00\"\n", i,
            sl->logical_len + 1, sl->buf);
  }

  cg_free(&cg);
  fclose(f);
  return 0;
}

/* build: clang directly from .ll */

static int run_cmd(const char *cmd)
{
  int rc = system(cmd);
  if (rc != 0)
    fprintf(stderr, "rigg: command failed: %s\n", cmd);
  return rc;
}

static int build(const Project *proj, const char *ir_dir, const char *out_path,
                 const CodegenOptions *opts)
{
#ifndef RIGG_RUNTIME_PATH
#define RIGG_RUNTIME_PATH "../runtime/cast.c"
#endif
  /* clang accepts .ll files directly — no llc step needed */
  int len = snprintf(NULL, 0, "clang %s -Wno-override-module -o %s %s", opts->opt_level, out_path,
                     RIGG_RUNTIME_PATH);
  for (int i = 0; i < proj->concept_count; i++)
    len += snprintf(NULL, 0, " %s/%s.ll", ir_dir, proj->concepts[i].name);

  char *cmd = xmalloc((size_t) len + 1);
  int pos = snprintf(cmd, (size_t) len + 1, "clang %s -Wno-override-module -o %s %s",
                     opts->opt_level, out_path, RIGG_RUNTIME_PATH);
  for (int i = 0; i < proj->concept_count; i++)
    pos += snprintf(cmd + pos, (size_t) len + 1 - (size_t) pos, " %s/%s.ll", ir_dir,
                    proj->concepts[i].name);

  int rc = run_cmd(cmd);
  free(cmd);
  return rc;
}

/* public entry point */

int codegen_run(const Project *proj, const CodegenOptions *opts)
{
  char ir_dir[4096];
  snprintf(ir_dir, sizeof(ir_dir), "%s/build/ir", proj->root);
  mkdir_p(ir_dir);

  for (int i = 0; i < proj->concept_count; i++)
  {
    char *out_path = xsprintf("%s/%s.ll", ir_dir, proj->concepts[i].name);
    int bounds_check = !opts->unsafe;
    int rc = emit_concept(proj, i, out_path, opts->target_triple, bounds_check);
    free(out_path);
    if (rc < 0)
      return -1;
  }

  if (opts->emit_ir_only)
    return 0;

  char *out_path = xsprintf("%s/build/out", proj->root);
  int rc = build(proj, ir_dir, out_path, opts);
  free(out_path);
  return rc;
}
