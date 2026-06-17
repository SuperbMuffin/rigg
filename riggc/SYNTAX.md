# Rigg Syntax Reference

## Primitives

```
i8   i16   i32   i64
u8   u16   u32   u64
f32  f64
bool
str
```

No implicit numeric conversions. Types must match exactly at call sites and assignments.

---

## Variables

```
let x: i32 = 10;
let name: str = "hello";
let flag: bool = true;
```

Variables are immutable by default.

```
let mut count: i32 = 0;
count = count + 1;
```

Use `mut` to declare a mutable variable.

---

## Constants

```
const MAX_SIZE: i32 = 256;
const PI: f64 = 3.14159;
```

Constants must be known at compile time. They are always immutable.

---

## Functions

```
fn add(a: i32, b: i32) -> i32
{
    return a + b;
}
```

Functions with no return value omit the `->` entirely.

```
fn greet(name: str)
{
    print(name);
}
```

The primary public function in a `.fn` file must share the filename.

Example — file `add.fn` must declare `fn add(...)` as its primary function.

Private helper functions may appear above the primary function.

```
fn clamp_value(x: i32, lo: i32, hi: i32) -> i32
{
    if x < lo { return lo; }
    if x > hi { return hi; }
    return x;
}

fn normalize(x: i32) -> i32
{
    return clamp_value(x, 0, 100);
}
```

Only `normalize` is visible outside the concept. `clamp_value` is private to the file.

---

## Cross-Concept Calls

Functions from another concept are called with `::` qualification.

```
fn run()
{
    buffer::load();
    renderer::draw();
}
```

The left side must be a concept name permitted by the project graph.
Unqualified calls resolve within the current concept only.

---

## Operators

### Arithmetic

```
a + b
a - b
a * b
a / b
a % b
```

### Comparison

```
a == b
a != b
a <  b
a >  b
a <= b
a >= b
```

### Logical

```
a && b
a || b
!a
```

---

## Control Flow

### If

```
if condition
{
    ...
}
```

```
if condition
{
    ...
}
else
{
    ...
}
```

```
if condition
{
    ...
}
else if other_condition
{
    ...
}
else
{
    ...
}
```

Conditions do not require parentheses.

### While

```
while condition
{
    ...
}
```

### Loop

```
loop
{
    ...
    if done { break; }
}
```

`loop` runs indefinitely until a `break` is reached.

### Break and Continue

```
break;
continue;
```

Valid only inside `while` or `loop` blocks.

---

## Return

```
return value;
return;
```

All code paths in a non-void function must return a value.
The compiler rejects functions with missing returns.

---

## Comments

```
// Single line comment

/*
   Multi-line comment
*/
```

---

## Entry Point

The root `main.fn` declares the program entry point.

```
fn main()
{
    editor::run();
}
```

`main` takes no arguments. It may optionally declare a return type.

```
fn main() -> i32
{
    editor::run();
    return 0;
}
```

---

## Reserved Words

```
fn       let      mut      const
if       else     while    loop
break    continue return   true
false
```

---

## File and Naming Rules

- `.fn` files export exactly one public function matching the filename.
- `.impl` files contain shared internal helpers, visible only within the owning concept.
- Concept names, function names, and variable names use `snake_case`.
- Type names use the primitive keywords above; user-defined types are not in scope for the prototype.
