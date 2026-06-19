# Rigg Compiler Errors

All reported errors are fatal. Project graph loading errors stop compilation
immediately. Semantic checking may report multiple structured errors before
compilation exits.

Error messages follow this format:

```
Error [CODE]: <message>

  --> <file>:<line>

<context if applicable>
```

---

## Graph Errors (G)

`project.meta` parse failures are currently printed directly by the project
loader, for example:

```text
rigg: project.meta:4: invalid concept name 'BadName'
rigg: project.meta:7: unknown concept 'utils'
rigg: project.meta:9: duplicate concept 'buffer' (first declared on line 3)
```

The structured graph error currently emitted by the compiler is:

### G002 — Circular dependency

The project graph contains a cycle.

```
Error G002: Circular dependency detected

  buffer -> rope -> buffer
```

---

## Concept Errors (C)

### C001 — Concept directory missing

A non-`main` concept referenced in `project.meta` has no directory.

```
Error C001: Missing concept directory 'rope'

  'rope' is declared in project.meta but no directory was found.
```

### C002 — Undeclared concept

A concept directory exists but is not declared in `project.meta`.

```
Error C002: Undeclared concept 'helpers'

  Directory 'helpers/' exists but is not declared in project.meta.
```

---

## Import Errors (I)

### I001 — Illegal cross-concept call

A function calls into a concept not permitted by the project graph.

```
Error I001: Illegal cross-concept call

  --> editor/run.fn:12

  editor -> core is not declared in project.meta.
```

### I002 — Unknown concept in call

A `::` call references a concept that does not exist.

```
Error I002: Unknown concept 'utils'

  --> editor/run.fn:8

  No concept named 'utils' exists in this project.
```

### I003 — Unknown function in concept

A `::` call references a function that is not exported by the target concept.

```
Error I003: Unknown function 'buffer::flush'

  --> editor/run.fn:15

  'flush' is not exported by concept 'buffer'.
  Exported functions: init, insert, delete, read
```

### I004 — Accessing internal implementation

A call attempts to reference a function defined in a `.impl` file from outside the concept.

```
Error I004: Cannot access internal function 'gap_buffer_shift'

  --> editor/run.fn:20

  'gap_buffer_shift' is defined in buffer/gap_buffer.impl and is not visible outside concept 'buffer'.
```

### I005 — Unknown function in unqualified call

An unqualified call or identifier refers to a name not defined in the current concept.

```
Error I005: Unknown function 'flush'

  --> buffer/init.fn:9

  'flush' is not defined in this concept.
```

```
Error I005: Unknown identifier 'value'

  --> buffer/init.fn:9

  'value' is not defined in this concept.
```

---

## File Errors (F)

### F001 — Missing primary function

A `.fn` file does not declare a public function matching its filename.

```
Error F001: Missing primary function in 'add.fn'

  Expected a function named 'add'.
```

### F002 — Duplicate primary function

A `.fn` file declares the primary function name more than once.

```
Error F002: Multiple public functions in 'add.fn'

  --> math/add.fn:8

  Only one function named 'add' is allowed per .fn file.
  Move helpers above the primary function or into a .impl file.
```

### F003 — Filename does not match function name

The primary function in a `.fn` file does not match the filename.

```
Error F003: Function name mismatch in 'add.fn'

  --> math/add.fn:1

  Expected 'fn add(...)' but found 'fn subtract(...)'.
```

---

## Syntax Errors (S)

### S001 — Unexpected token

The parser encountered a token it did not expect. This also covers missing
expected punctuation such as `;` or `}`.

```
Error S001: Unexpected token '}'

  --> buffer/init.fn:14
```

### S004 — Break or continue outside loop

`break` or `continue` appears outside a `while`, `for`, or `loop` block.

```
Error S004: 'break' used outside of a loop

  --> buffer/init.fn:7
```

### S005 — Bare return in non-void function

A `return;` with no value appears in a function that declares a return type.

```
Error S005: Bare return in non-void function 'add'

  --> math/add.fn:5

  Function 'add' declares return type i32 but 'return' has no value.
```

---

## Type Errors (T)

### T001 — Type mismatch

A value of the wrong type was used in an expression or assignment.

```
Error T001: Type mismatch

  --> math/add.fn:6

  Expected i32, found f32.
```

### T002 — Unknown type

An unrecognised type name was used where a type was expected.

```
Error T002: Unknown type 'int'

  --> math/add.fn:1

  expected a type but got 'int'
```

### T003 — Missing return

A function with a return type does not return a value on all code paths.

```
Error T003: Missing return in 'add'

  --> math/add.fn

  Function declares return type i32 but not all code paths return a value.
```

### T004 — Return in void function

A void function attempts to return a value.

```
Error T004: Unexpected return value in 'greet'

  --> core/greet.fn:5

  Function has no return type but returns a value.
```

### T005 — Reassignment of immutable variable

A variable declared without `mut` is assigned a new value.

```
Error T005: Reassignment of immutable variable 'x'

  --> buffer/init.fn:8

  'x' was declared without 'mut'.
  Change to: let mut x: i32 = ...
```

---

## Entry Point Errors (E)

### E001 — Missing entry point

The project has no `main` concept in `project.meta`, or the `main` concept is
declared but root `main.fn` is missing.

```
Error E001: Missing entry point

  No 'main' concept found in project.meta.
  Add 'main:' with its dependencies and a root-level main.fn.
```

```
Error E001: Missing entry point

  --> project.meta:1

  'main' is declared in project.meta but main.fn was not found.
```

### E002 — Invalid main signature

`main` declares parameters or returns a type other than `i32`.

```
Error E002: Invalid main signature

  --> main.fn:1

  'main' must take no arguments.
  Expected: fn main() or fn main() -> i32
```

```
Error E002: Invalid main signature

  --> main.fn:1

  'main' may only return i32 or nothing, found 'bool'.
```
