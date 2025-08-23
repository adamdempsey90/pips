# pips
Header-only compiler for a simple programming language. 

Much of this code is based on the clox language from the book
"Crafting Interpreters" by Robert Nystrom
https://craftinginterpreters.com/contents.html. The code for the book is available at 
https://github.com/munificent/craftinginterpreters under the MIT License.

Compared to the book, `pips` is simplified in several ways (e.g., string handling) and was translated to C++.

This project is very much a work in progress.

## Compiling

`pips` is a CMake project and is mostly meant to be included in other projects as a library. 
This repository contains a standalone executable that provides a REPL interface. 
To compile, execute:
```bash
mkdir build && cd build
cmake .. && make
./repl/repl
```