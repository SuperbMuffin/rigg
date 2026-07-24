# Rigg Monorepo

This repository contains the full Rigg language toolchain.

It is split into:

- `riggc` - compiler
- `rigg` - CLI and project orchestration tool
- `core` - standard library for the Rigg language
- `examples` - examples of rigg being used

Check individual READMEs for more information on each of these components and how to contribute and use them.

<details>
<summary>
  
## Basic Overview and Examples

</summary>

### Philosophy 

The `rigg` languages philosophy in its most basic form is to make **a project as easily scannable and understandable as possible**. It tries to accomplish that through the `project.meta` dependency tree aswell as `concepts` and the fact that the public api is defined by the files inside those `concepts.

### What is a concept?
A concept is a folder containing `.fn` and maybe `.impl` files. Each `.fn` file is a public function accessible by other concepts e.g. the main concept. Inside the `.fn` files **one** function must be defined by the name of the file e.g. `add.fn` needs `add` to be a function otherwise a compilation error is "thrown". In `.impl` files on the otherhand all defined functions are accessible by the concept exclusively. Throwing this all together it could look like:
```
main.fn
math/
  add.fn
  sub.fn
  normalize.impl
```
then main.fn can call `add` and `sub` but not what `normalize` defines. `main.fn` is special because its the entrypoint but it is also a concept at its most basic form.

### `project.meta` and `project.toml`???
Both might seem like a similar thing but they serve a different purpose, beginning with the more simple one `project.toml` defines all the metadata for the project aswell as the compile details e.g. optimzation level.

`project.meta` is a file that describes the relationships of concepts to eachother. It will tell you what depends on what and how to think about the codebase. Sticking to our basic `math/` example a `project.meta` could look like this:
```
math

main:
-> math
```
That tells you math doesn't depend on anything and main requires math. The compiler automatically stops circular dependcies and all that stuff.

### Core library
The core library contains branches like `core_io` and `core_str`. The core library is similar to the standard library of many othe langauges, it provides you functions like `println` and `concat`. Core libraries are written in `rigg` and are concepts just like any you could write. They do however extern to the c standard library quite often. Currently the core libraries need to be symlinked relatively currently but in the newr future you should just be able to run `rigg add core_xxxx` 

### Examples and Syntax
Since the language is always adapting currently im not going to write too many examples in here, refer to [examples/README.md](./examples/README.md) for more examples. Also check out [riggc/README.md](./riggc/README.md) for a super in depth description of **everything** rigg.

For now ill just leave you with a calculator:
```
fn main()
{
  let a: str = core_io::input("First Num: ");
  let op: str = core_io::input("Operation: ");
  let b: str = core_io::input("Second Num: ");

  if (op == "+")
  {
    let result: str = ((a as f32) + (b as f32)) as str;
    let msg: str = core_str::concat("Result: ", result);
    core_io::println(msg);
  }
  
  else if (op == "-")
  {
    let result: str = ((a as f32) - (b as f32)) as str;
    let msg: str = core_str::concat("Result: ", result);
    core_io::println(msg);
  }

  else if (op == "*")
  {
    let result: str = ((a as f32) * (b as f32)) as str;
    let msg: str = core_str::concat("Result: ", result);
    core_io::println(msg);
  }

  else if (op == "/")
  {
    let result: str = ((a as f32) / (b as f32)) as str;
    let msg: str = core_str::concat("Result: ", result);
    core_io::println(msg);
  }
  else
  {
    core_io::println("Input a real operator");
  }

}
```
That code of course needs the `core_io` and `core_str` concepts. Check out [this](./examples/calc/) for the full version.
</details>


> [!IMPORTANT]
> Unlike everything else in this repo, the `riggc` compiler is licensed under the `MPL-2.0` license while everything else is under the MIT license.
