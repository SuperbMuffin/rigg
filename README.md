# Rigg Monorepo

This repository contains the full Rigg language toolchain.

It is split into:

- `riggc` - compiler
- `rigg` - CLI and project orchestration tool
- `core` - standard library for the Rigg language
- `examples` - examples of rigg being used

Check individual READMEs for more information on each of these components and how to contribute and use them.

> [!WARNING]
> In the current state of this project there is no way for the language to handle the symlinking of the core library into the project leading to the inability to have a real project written in this language, would not recommand regardless since the language is still very alpha. The `examples` folder handles this through the relative symlinking of the core library. That however is not elegent and is temporary 


> [!IMPORTANT]
> Unlike everything else in this repo, the `riggc` compiler is licensed under the `MPL-2.0` license while everything else is under the MIT license.
