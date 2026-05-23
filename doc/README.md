# pips documentation

This folder contains the user-facing language notes for the current `pips` implementation.

## Start here

- [Language guide](language.md): syntax, constructs, expressions, statements, functions, classes, and vectors.
- [Embedding from C++](embedding.md): compile scripts in a `VM`, call named functions from host code, and handle `Value` arguments and returns.
- [Running code](running.md): how the REPL, `-c` snippets, and `-i` files differ in practice.
- [Examples](examples.md): runnable examples chosen to collectively exercise the whole current VM instruction set.

## Notes

- The language is still evolving, so these docs describe the implementation in this repository today.
- Classes are script-scope declarations; they cannot currently be declared inside function bodies.
