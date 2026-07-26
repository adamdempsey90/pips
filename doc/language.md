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

Strings also provide immutable utility methods. All transforming methods
return new values; they do not modify the receiver:

- `s.len()` and `s.size()` return the byte length.
- `s.lower()` and `s.upper()` perform ASCII case conversion.
- `s.strip([chars])`, `s.lstrip([chars])`, and `s.rstrip([chars])` trim ASCII
  whitespace by default, or characters from the supplied string.
- `s.starts_with(prefix)`, `s.ends_with(suffix)`, and `s.contains(text)` return
  booleans.
- `s.replace(old, replacement)` replaces all non-overlapping matches. `old`
  must not be empty.
- `s.split()` splits collapsed ASCII whitespace; `s.split(separator)` uses a
  non-empty literal separator and preserves empty fields.

These operations work on bytes rather than Unicode code points.

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

Vectors provide stack-like methods:

```pips
print(v.len(), v.size());
var ignored = v.push(50); # mutates v; ignored is nil
var removed = v.pop();    # mutates v; removed is 50
print([1, 2, 3].join(" + ")); # "1 + 2 + 3"
```

Host vectors have shared-reference semantics, so mutations are visible through
aliases. `pop()` on an empty vector is a runtime error. The host-only
`v.join(separator)` method converts each scalar element using the same rules as
`str(value)`, places the required string separator between the elements, and
returns a new string. Joining an empty vector returns an empty string.

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

## Vector generator built-ins

These functions create a new vector without needing a literal.

### `range`

```pips
range(stop)              # [0, 1, ..., stop-1]  (step = 1)
range(start, stop)       # [start, ..., stop-1] (step = 1)
range(start, stop, step) # [start, start+step, ...] stopping before stop
```

`step` may be negative; it must not be zero. All arguments are treated as
numerics and need not be integers.

```pips
var a = range(5);              # [0, 1, 2, 3, 4]
var b = range(2, 6);           # [2, 3, 4, 5]
var c = range(0, 1, 0.25);     # [0, 0.25, 0.5, 0.75]
var d = range(3, 0, -1);       # [3, 2, 1]
```

### `linspace`

```pips
linspace(start, stop, count)   # count evenly-spaced points from start to stop inclusive
```

```pips
var t = linspace(0, 1, 5);     # [0, 0.25, 0.5, 0.75, 1]
```

`count = 0` returns an empty vector; `count = 1` returns `[start]`.

### `logspace`

```pips
logspace(start, stop, count)   # count points: exp(linspace(start, stop, count))
```

Arguments are exponents in natural-log space. The resulting values are `e^start, ..., e^stop`.

```pips
var v = logspace(0, 1, 3);     # [1, e^0.5, e] ≈ [1, 1.6487, 2.7183]
```

### `log10space`

```pips
log10space(start, stop, count) # count points: 10^linspace(start, stop, count)
```

Arguments are exponents in base-10 space. The resulting values are `10^start, ..., 10^stop`.

```pips
var v = log10space(0, 3, 4);   # [1, 10, 100, 1000]
```

### `zeros` and `ones`

```pips
zeros(count)   # vector of count zeros
ones(count)    # vector of count ones
```

```pips
var z = zeros(4);   # [0, 0, 0, 0]
var o = ones(3);    # [1, 1, 1]
```

## Introspection helpers

These are statement forms rather than functions:

- `__list__` shows the current variables
- `__globals__` shows globals
- `__locals__` shows locals in the current function frame
- `__stack__` shows the VM stack
- `__funcs__` shows the registered functions

See [Examples](examples.md) for runnable snippets that use each of these constructs.
