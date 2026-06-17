# rigg — Rigg CLI

`rigg` is the command-line interface for the Rigg language toolchain. It is responsible for project management, compilation orchestration, and developer workflows.

It acts as the primary user-facing entry point into the Rigg ecosystem.

---

## Overview

`rigg` does not implement compilation itself. Instead, it coordinates and invokes the compiler backend (`riggc`) and manages project structure, builds, tests, and execution.

---

## Commands

### `rigg init`

Initializes a new Rigg project in the current directory.

Creates a standard project layout and fills them with sane defaults:

```
main.fn
project.meta
```

---

### `rigg new <name>`

Creates a new project directory and initializes it.

```
rigg new my_project
```

which looks like:

```
my_project/
    src/
        main.fn
        project.meta
    README.md
```
---

### `rigg build`

Builds the current project using the compiler backend.

Outputs compiled artifacts into the build directory.

---

### `rigg run`

Builds and executes the current project.

Equivalent to:

```
rigg build && ./build/output
```

---

### `rigg check`

Performs compilation checks without generating final output.

Used for fast validation of syntax and type correctness.

---

### `rigg test`

Runs the project test suite.

Discovers and executes tests under the `tests/` directory.

---

## Architecture

`rigg` is strictly a frontend tool. It delegates compilation work to `riggc`.

```
rigg (CLI)
  → riggc (compiler backend)
      → core (standard library)
```

---

## Design Principles

* No hidden behavior
* No implicit dependencies
* Explicit project structure
* Deterministic builds
* Unix-first design

