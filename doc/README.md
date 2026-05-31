# pips documentation

This folder contains the user-facing language notes for the current `pips` implementation.

## Start here

- [Language guide](language.md): syntax, constructs, expressions, statements, functions, classes, vectors, and built-in generator functions.
- [Embedding from C++](embedding.md): compile scripts in a `VM`, call named functions from host code, and handle `Value` arguments and returns.
- [Calling functions on device](device.md): pack a script function into a `DeviceModule`, upload it to CUDA or HIP, and execute it in a kernel with `DeviceVM`.
- [Running code](running.md): how the REPL, `-c` snippets, `-i` files, and the standalone ODE solver application differ in practice.
- [Examples](examples.md): runnable examples chosen to collectively exercise the whole current VM instruction set, including the vector generator builtins.

## Notes

- The language is still evolving, so these docs describe the implementation in this repository today.
- Classes are script-scope declarations; they cannot currently be declared inside function bodies.
