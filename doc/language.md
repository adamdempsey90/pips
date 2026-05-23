# Language Guide

See also [Running code](running.md) and [Examples](examples.md).

## Values and literals

`pips` currently supports these literal forms:

- Numbers: `1`, `3.14`, `-2`
- Strings: `"hello"`
- Booleans: `true`, `false`
- Nil: `nil`
- Built-in constant: `pi`
- Vectors: `[1, 2, 3]`

## Variables

Declare variables with `var`:

```pips
var x = 2;
var name = "pips";
var empty = nil;
```

At top level this defines globals. Inside a block or function it defines locals.

## Expressions and operators

### Unary operators

- Numeric sign: `-x`, `+x`
- Logical not: `!x` or `not x`
- Bitwise not: `~x`
- Prefix increment and decrement: `++x`, `--x`

### Binary operators

- Arithmetic: `+`, `-`, `*`, `/`, `//`, `%`, `**`
- Comparison: `==`, `!=`, `>`, `>=`, `<`, `<=`
- Logical: `and`, `or`
- Bitwise: `^`, `|`, `&`, `<<`, `>>`
- Ternary: `condition ? when_true : when_false`

### Assignment forms

- Simple assignment: `x = expr`
- Compound assignment: `+=`, `-=`, `*=`, `/=`, `%=`, `|=`, `&=`, `<<=`, `>>=`
- Postfix increment and decrement: `x++`, `x--`

## Statements and control flow

### Printing

Use `print(...)` to print one or more expressions:

```pips
print(x);
print("x = ", x, ", y = ", y);
```

### Blocks

Blocks introduce a nested scope:

```pips
{
  var inner = 42;
  print(inner);
}
```

### Conditionals

```pips
if (x > 0) {
  print("positive");
} else {
  print("not positive");
}
```

### Loops

```pips
while (x < 10) {
  x += 1;
}

for (var i = 0; i < 3; i++) {
  print(i);
}
```

## Functions

Functions are declared with `fn`:

```pips
fn add(a, b) {
  return a + b;
}
```

Functions can be nested. Inner functions can read and update locals from an enclosing function:

```pips
fn counter(start) {
  var value = start;
  fn bump(step) {
    value += step;
    return value;
  }
  return bump(1);
}
```

## Classes and instances

Classes are declared at script scope:

```pips
class Point {
  var x = 0;
  var y = 0;

  fn magnitude2() {
    return x * x + y * y;
  }
}
```

Create an instance with `new` and an initializer block:

```pips
var p = new Point {
  .x = 3;
  .y = 4;
}
```

Access fields and methods with `.`:

```pips
print(p.x);
p.y = 10;
print(p.magnitude2());
```

Inside methods:

- `this` refers to the current instance.
- Bare field names like `x` and `y` resolve to fields on `this`.
- Bare method calls like `magnitude2()` resolve to methods on `this`.

Dynamic attribute helpers are also available:

```pips
setattr(p, "label", "origin");
print(getattr(p, "label"));
print(hasattr(p, "x"));
```

## Strings

Strings are first-class values and can be concatenated with `+`:

```pips
var greeting = "hello";
var target = "world";
print(greeting + " " + target);
```

Use `str(value)` to convert values to strings.

## Vectors, indexing, and slicing

Vector literals use square brackets:

```pips
var v = [10, 20, 30, 40];
```

Indexing and assignment:

```pips
print(v[0]);
v[1] = 99;
```

Slices use `start:end` and support omitted endpoints:

```pips
print(v[1:3]);
print(v[:2]);
print(v[2:]);
v[1:3] = [7, 8];
```

## Built-in functions

Unary numeric built-ins:

- `exp(x)`
- `sin(x)`
- `cos(x)`
- `tan(x)`
- `abs(x)`
- `log(x)`
- `log10(x)`
- `sign(x)`
- `sqrt(x)`
- `acos(x)`
- `asin(x)`
- `atan(x)`
- `ceil(x)`
- `floor(x)`
- `env(x)`

Binary numeric built-ins:

- `atan2(a, b)`
- `min(a, b)`
- `max(a, b)`

`env(name)` looks up an environment variable and returns either a number or a string depending on its contents.

## Introspection helpers

These are statement forms rather than functions:

- `__list__` shows the current variables
- `__globals__` shows globals
- `__locals__` shows locals in the current function frame
- `__stack__` shows the VM stack
- `__funcs__` shows the registered functions

See [Examples](examples.md) for runnable snippets that use each of these constructs.
