# Embedding From C++

See also [Language guide](language.md), [Running code](running.md), and [Examples](examples.md).

## Overview

`pips` can be embedded directly in a C++ program through the header-only `VM` type.

A typical flow is:

1. Construct a `pips::VM`
2. Compile and run a script with `interpret(...)` so named functions are registered
3. Call a compiled function from C++ with `VM::call(...)`
4. Read the returned `Value`

## Basic example

```cpp
#include <pips/vm.hpp>
#include <vector>
#include <string>

using namespace pips;

int main() {
  VM vm;

  const char *source =
      "fn add(a, b) { return a + b; }\n"
      "fn echo(s) { return s; }\n";

  if (vm.interpret(source) != InterpretResult::OK) {
    return 1;
  }

  Value sum = vm.call("add", std::vector<Value>{NUMBER_VAL(2), NUMBER_VAL(3)});
  Value text = vm.call("echo", std::vector<Value>{vm.makeString("hello")});

  if (IS_NUMBER(sum)) {
    printf("sum = %.0Lf\n", AS_NUMBER(sum));
  }
  if (IS_STRING(text)) {
    printf("text = %s\n", AS_STRING(text));
  }

  return 0;
}
```

## Relevant API

### `InterpretResult interpret(const char *source, char end_line = ';')`

Compiles and executes a source string. Top-level named functions defined by that source are stored in the VM and can be called later.

### `Value call(const std::string &name, const std::vector<Value> &args)`

Looks up a previously compiled function by name, pushes the provided arguments, executes the function, and returns its result as a `Value`.

### `Value makeString(std::string s)` and `Value makeString(const char *s)`

Create a VM-owned string value suitable for passing as a function argument.

This matters because string values in `pips` are backed by VM-managed `StringObject` storage.

## Argument and return values

`Value` already supports the scalar runtime types used by the interpreter.

Common host-side patterns:

```cpp
Value n = NUMBER_VAL(42);
Value b = BOOL_VAL(true);
Value z = NIL_VAL;
Value s = vm.makeString("hello");
```

You can inspect returned values with the existing `IS_*` and `AS_*` macros:

```cpp
if (IS_NUMBER(result)) {
  auto x = AS_NUMBER(result);
}
if (IS_STRING(result)) {
  auto s = AS_STRING(result);
}
```

## Current limitations

### Only named compiled functions

`call()` works with functions that have already been compiled into the VM's function table.

### No true shipped closures

Nested functions are not standalone closure objects in the current runtime. A nested function that accesses outer locals still depends on an active enclosing call stack frame, so it cannot be extracted and shipped around like a self-contained callable.

### `nil` is also the failure sentinel

At the moment, `call()` returns `nil` both for a legitimate `nil` result and for lookup/arity/runtime failures after printing an error message to stderr. If host code needs to distinguish those cases, the API will need an additional status channel.

### Methods are different

This API is for calling named functions. It is not a direct host API for instance methods.

## Practical guidance

- Reuse the same `VM` instance for compilation and calls.
- Create string arguments with `vm.makeString(...)`.
- Treat `call()` as a top-level named-function API, not as a general function-object system.
- If you need reliable host-side error handling, add a status-returning overload before depending on `nil` results.
