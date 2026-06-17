# Rigg Compiler Errors

All errors are fatal. Compilation stops on the first error unless otherwise noted.

Error messages follow this format:

```
Error [CODE]: <message>

  --> <file>:<line>

<context if applicable>
```

---

## Graph Errors (G)

### G001 — Invalid graph syntax

`project.meta` could not be parsed.

```
Error G001: Invalid graph syntax

  --> project.meta:4
```

### G002 — Circular dependency

The project graph contains a cycle.

```
Error G002: Circular dependency detected

  buffer -> rope -> buffer
```

### G003 — Unknown concept in graph

A concept is referenced in `project.meta` but has no matching directory.

```
Error G003: Unknown concept 'utils'

  --> project.meta:7

  'utils' is declared in the graph but no concept directory was found.
```

### G004 — Duplicate concept in graph

A concept is declared more than once in `project.meta`.

```
Error G004: Duplicate concept 'buffer'

  --> project.meta:9

  'buffer' was already declared on line 3.
```

---

## Concept Errors (C)

### C001 — Concept directory missing

A concept referenced in `project.meta` has no directory.

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

An unqualified call refers to a function not defined anywhere in the current concept.

```
Error I005: Unknown function 'flush'

  --> buffer/init.fn:9

  'flush' is not defined in this concept.
```

---

## File Errors (F)

### F001 — Missing primary function

A `.fn` file does not declare a public function matching its filename.

```
Error F001: Missing primary function in 'add.fn'

  Expected a function named 'add'.
```

### F002 — Multiple public functions

A `.fn` file declares more than one function that could be considered public.

```
Error F002: Multiple public functions in 'add.fn'

  --> math/add.fn:8

  Only one public function is allowed per .fn file.
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

The parser encountered a token it did not expect.

```
Error S001: Unexpected token '}'

  --> buffer/init.fn:14
```

### S002 — Missing semicolon

A statement is missing a terminating semicolon.

```
Error S002: Missing semicolon

  --> buffer/init.fn:9
```

### S003 — Unclosed brace

A block was opened but never closed.

```
Error S003: Unclosed brace

  --> buffer/init.fn:3
```

### S004 — Break or continue outside loop

`break` or `continue` appears outside a `while` or `loop` block.

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

An unrecognised type name was used.

```
Error T002: Unknown type 'int'

  --> math/add.fn:1

  Did you mean 'i32'?
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

No `main.fn` exists at the project root.

```
Error E001: Missing entry point

  No main.fn found at project root.
```

### E002 — Invalid main signature

`main` declares parameters.

```
Error E002: Invalid main signature

  --> main.fn:1

  'main' must take no arguments.
  Expected: fn main() or fn main() -> <type>
```
