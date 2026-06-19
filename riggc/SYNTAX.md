# Rigg Syntax Reference

## Primitives

```
i8   i16   i32   i64
u8   u16   u32   u64
f32  f64
bool
str
ptr
```

Types generally must match at call sites and assignments. Integer literals may
be typed by their surrounding context, so `let x: i8 = 1;` is valid.

Explicit casts are written with `as`.

```
let n: i32 = "42" as i32;
let s: str = n as str;
let p: ptr = s as ptr;
```

Supported casts include signed integer conversions, `str`/`ptr`
representation casts, `str`/signed-integer conversions, signed-integer/`ptr`
conversions, and `bool`/`str` conversions.

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

Constants are immutable variables. Their initializer is type-checked like a
`let` initializer.

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

## Externs

External functions and variables are declared at top level with `extern`.

```
extern fn printf(fmt: str, ...) -> i32;
extern var errno: i32;
```

Variadic extern functions use `...`. Only the fixed arguments are type-checked.

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

Unary numeric negation is also supported:

```
-a
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

### Grouping

```
(a + b) * c
```

### Cast

```
value as i32
ptr_value as str
```

`as` binds looser than binary operators, so `a + b as str` parses as
`(a + b) as str`.

### Pointer Indexing

```
let byte: i32 = p[0];
p[0] = 65;
```

Indexing requires a `ptr` target and an integer index. Pointer indexing reads
and writes values typed as `i32` at the language level.

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

### For

```
for (let mut i: i32 = 0; i < n; i++)
{
    ...
}
```

The init clause is a `let` declaration, the condition is a boolean
expression, and the post clause is an assignment or `i++` / `i--`.

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

Valid only inside `while`, `for`, or `loop` blocks.

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

The root `main.fn` declares the program entry point and the project graph must
declare a `main` concept.

```
main:
  -> editor
```

```
fn main()
{
    editor::run();
}
```

`main` takes no arguments. It may return `i32` or omit the return type.

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
break    continue return   extern
var      as       true     false
```

---

## File and Naming Rules

- `.fn` files export exactly one public function matching the filename.
- `.impl` files contain shared internal helpers, visible only within the owning concept.
- Concept names, function names, and variable names use `snake_case`.
- Type names use the primitive keywords above; user-defined types are not in scope for the prototype.
