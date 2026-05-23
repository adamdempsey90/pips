# pips
[![GCC Build](https://github.com/adamdempsey90/pips/actions/workflows/gcc-build.yml/badge.svg)](https://github.com/adamdempsey90/pips/actions/workflows/gcc-build.yml)
[![Clang Build](https://github.com/adamdempsey90/pips/actions/workflows/clang-build.yml/badge.svg)](https://github.com/adamdempsey90/pips/actions/workflows/clang-build.yml)

Header-only compiler for a simple programming language. 

Much of this code is based on the clox language from the book
"Crafting Interpreters" by Robert Nystrom
https://craftinginterpreters.com/contents.html. The code for the book is available at 
https://github.com/munificent/craftinginterpreters under the MIT License.

Compared to the book, `pips` is simplified in several ways (e.g., string handling) and was translated to C++.

This project is very much a work in progress.

## Documentation

- [Documentation index](doc/README.md)
- [Language guide](doc/language.md)
- [Embedding from C++](doc/embedding.md)
- [Calling functions on device](doc/device.md)
- [Running code](doc/running.md)
- [Examples](doc/examples.md)

## Compiling

`pips` is a CMake project and is mostly meant to be included in other projects as a library. 
This repository contains a standalone executable that provides a REPL interface. 
To compile, execute:
```bash
mkdir build && cd build
cmake .. && make
./repl/repl
```