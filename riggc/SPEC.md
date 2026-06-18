# Rigg Architecture Specification (Draft)

## Philosophy

The language is designed around explicit project structure.

The filesystem defines:

* Public API surface
* Concept boundaries
* Internal implementation visibility

The project graph defines:

* Allowed dependencies between concepts
* Architectural constraints
* Circular dependency prevention

The goal is for a developer to understand a project by reading:

1. The project graph
2. The concept directories
3. The public function files

without needing to inspect implementation details.

---

# Project Layout

A project consists of:

```text
project.meta

main.fn

concept_a/
concept_b/
concept_c/
```

Example:

```text
project.meta

main.fn

math/
core/
rope/
buffer/
terminal/
editor/
```

---

# Entry Point

The root `main.fn` file is the program entry point, but it is loaded through
the `main` concept declared in `project.meta`.

A runnable project must contain both:

```text
main:
  -> editor
```

and a root-level `main.fn`.

Example:

```text
fn main()
{
    editor::run();
}
```

Compilation begins by loading the project graph, finding the `main` concept,
and then loading root `main.fn`.

---

# Concepts

A concept is normally a directory. The special `main` concept maps to the
root-level `main.fn` file instead of a `main/` directory.

Example:

```text
math/
geometry/
editor/
```

Concepts are the primary architectural unit of the language.

Concepts may depend on other concepts only if permitted by the project graph.

---

# Public Function Files

A `.fn` file exposes exactly one public function.

Example:

```text
math/
    normalize.fn
```

The public function is:

```text
fn normalize(...)
```

A `.fn` file may contain:

* Private helper functions
* Internal implementation logic
* Local constants

Only the primary function is exported.

Example:

```text
fn helper_a(...)
fn helper_b(...)

fn normalize(...)
```

External concepts may only access `normalize`.

---

# Implementation Files

`.impl` files contain shared internal implementation code.

Example:

```text
math/
    normalize.fn
    clamp.fn
    arithmetic.impl
```

Rules:

* Visible only within the owning concept.
* Not importable outside the concept.
* May contain multiple helper functions.
* May not expose public API.

Purpose:

```text
normalize.fn
    uses arithmetic.impl

clamp.fn
    uses arithmetic.impl
```

---

# Visibility Rules

Visibility is determined by filesystem structure.

## Public

Any `.fn` file represents a public callable function.

Example:

```text
math/add.fn
```

Exports:

```text
add(...)
```

## Internal

`.impl` contents are concept-private.

External concepts cannot access them.

---

# Project Graph

The root `project.meta` file defines concept dependencies.

The graph is authoritative.

All imports must be allowed by the graph.

---

# Graph Syntax

Example:

```text
core

main:
  -> editor

rope:
  -> core

terminal:
  -> core

buffer:
  -> rope

editor:
  -> buffer
  -> terminal
```

Meaning:

* Rope depends on Core
* Terminal depends on Core
* Buffer depends on Rope
* Editor depends on Buffer and Terminal
* Main depends on Editor

---

# Dependency Rules

Dependencies are directional.

Example:

```text
editor:
  -> buffer
```

Allows:

```text
editor -> buffer
```

Disallows:

```text
buffer -> editor
```

unless explicitly declared.

---

# Circular Dependencies

The graph must be acyclic.

Illegal:

```text
buffer:
  -> rope

rope:
  -> buffer
```

Compiler error:

```text
Circular dependency detected:

buffer -> rope -> buffer
```

Compilation fails.

---

# Compilation Process

1. Read graph.
2. Validate graph syntax.
3. Detect cycles.
4. Validate concept directories.
5. Load root `main.fn` through the `main` concept.
6. Validate imports.
7. Type-check functions.
8. Convert to LLVM IR.
9. Compile concepts with LLVM.
10. Link executable.

---

# Architectural Principle

Concepts represent capabilities.

Good examples:

```text
rope
buffer
terminal
renderer
parser
lexer
editor
```

Poor examples:

```text
utils
helpers
misc
stuff
```

Concept boundaries should communicate responsibility.

---

# Example Editor Architecture

Graph:

```text
core

main:
  -> editor

terminal:
  -> core

rope:
  -> core

buffer:
  -> rope

cursor:
  -> buffer

viewport:
  -> buffer

input:
  -> terminal

renderer:
  -> terminal
  -> buffer
  -> cursor
  -> viewport

commands:
  -> buffer
  -> cursor

editor:
  -> buffer
  -> cursor
  -> viewport
  -> renderer
  -> input
  -> commands
```

This structure makes architecture explicit, prevents circular dependencies, and provides a high-level overview of the entire application.
