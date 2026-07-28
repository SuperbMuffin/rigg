# Plans
## Arrays

Arrays are fixed-size collections that store elements contiguously in memory. Arrays own their elements and are immutable by default, like all values in Rigg.

```rigg
let a: [i32; 3] = [1, 2, 3];
let mut b: [i32; 3] = [1, 2, 3];
```

The array size is part of the type.

```rigg
[i32; 3]
```

represents an array containing exactly 3 `i32` values.

Array literals infer their size automatically:

```rigg
let a = [1, 2, 3];
// inferred as [i32; 3]
```

When an explicit size is provided, the initializer length must match:

```rigg
let a: [i32; 3] = [1, 2, 3];      // OK
let b: [i32; 4] = [1, 2, 3, 4];   // OK
let c: [i32; 4] = [1, 2, 3];      // Error
```

---

### Mutability

Arrays are immutable by default.

```rigg
let a = [1, 2, 3];

a[0] = 5; // Error
```

Using `let mut` allows modifying elements through indexing.

```rigg
let mut a = [1, 2, 3];

a[0] = 5; // OK
```

Mutability applies to the binding, not the array type.

---

### Indexing

Arrays use zero-based indexing.

```rigg
let a = [10, 20, 30];

a[0]; // 10
a[2]; // 30
```

Indexing performs bounds checking.

If the index is known at compile time and is out of bounds, the compiler emits a compile-time error.

```rigg
let a = [1, 2, 3];

a[5]; // Compile-time error
```

If the index is only known at runtime, a bounds check is generated. If the index is invalid, the program traps.

```rigg
a[index];
```

Out-of-bounds access is never undefined behavior.

---

### Array Length

The built-in `len()` operation returns the number of elements in an array.

```rigg
let a = [1, 2, 3];

len(a); // 3
```

For fixed-size arrays, the result is known at compile time.

---

### Array Equality

Arrays compare element-by-element.

```rigg
[1, 2, 3] == [1, 2, 3] // true
[1, 2, 3] == [3, 2, 1] // false
```

---

### Copy Semantics

Arrays are value types.

```rigg
let a = [1, 2, 3];
let b = a;
```

If the element type is copyable, `b` receives its own copy of the array.

Arrays do not implicitly decay into pointers or references.

---

### Multidimensional Arrays

Arrays can contain other arrays.

```rigg
let board: [[i32; 3]; 3];
```

Indexing is performed one dimension at a time.

```rigg
board[0]      // [i32; 3]
board[0][0]   // i32
board[2][1]   // i32
```

No special multidimensional syntax is required.

---

### Empty Arrays

Zero-length arrays are valid.

```rigg
let empty: [i32; 0] = [];
```

This keeps the type system consistent and is useful for generic code.

---

### Future: Slices

Slices are a separate type from arrays.

```rigg
&[i32]
```

A slice is a non-owning view into contiguous memory and conceptually contains:

```text
pointer
length
```

The element type is known by the compiler and is not stored at runtime.

Arrays own their data. Slices borrow existing data and allow functions to operate on arrays of arbitrary length without copying.

---

### Deferred Features

The following features are intentionally postponed:

* Slice syntax (`array[start..end]`)
* Dynamic arrays
* Iterators
* Collection types
* Array methods (`map`, `filter`, etc.)
* Uninitialized arrays

The initial array implementation focuses on providing a simple, predictable fixed-size container that serves as the foundation for future collection types.

