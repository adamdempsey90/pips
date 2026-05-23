# Examples

See also [Language guide](language.md) and [Running code](running.md).

These examples are intentionally chosen so that, taken together, they exercise the full current VM instruction set: literals, arithmetic, comparisons, jumps, loops, function calls, closures, object operations, vector operations, indexing, slicing, and the built-in inspection statements.

## 1. Expressions, built-ins, and assignment

Runnable file: [examples/example1.pips](../examples/example1.pips)

```pips
var a = 7;
var b = 3;
var nothing = nil;
var yes = true;
var no = false;

print(-a, +a, !no, ~1);
print(a + b, a - b, a * b, a / b, a // b, a % b, a ** b);
print(a == b, a != b, a > b, a >= b, a < b, a <= b);
print(yes and no, yes or no, yes ^ no);
print(5 | 2, 6 & 3, 1 << 3, 8 >> 1);
print(yes ? "ternary-true" : "ternary-false");

print(exp(1), sin(pi / 2), cos(0), tan(0), abs(-3), log(exp(1)));
print(log10(100), sign(-9), sqrt(9), acos(1), asin(1), atan(1));
print(ceil(1.2), floor(1.8), atan2(0, -1), min(3, 4), max(3, 4));
print(env("HOME"), str(42));

a = a + 1;
a += 2;
a -= 1;
a *= 3;
a /= 2;
a %= 5;
a |= 2;
a &= 3;
a <<= 1;
a >>= 1;
a++;
++a;
a--;
--a;
print(a, nothing);
```

## 2. Control flow, functions, closures, and inspectors

Runnable file: [examples/example2.pips](../examples/example2.pips)

```pips
var answer = 0;

fn outer(limit) {
  var total = 0;

  fn bump(step) {
    total += step;
    return total;
  }

  bump(1);

  if (limit > 2) {
    total = total + 10;
  } else {
    total = total - 10;
  }

  var i = 0;
  while (i < limit) {
    total += i;
    i += 1;
  }

  for (var j = 0; j < 3; j++) {
    total += j;
  }

  __locals__;
  __stack__;

  return bump(2);
}

outer(4);
answer = outer(2);
print(answer);

__list__;
__globals__;
__funcs__;
```

Notes:

- The ignored `outer(4);` call is useful because it still executes but discards its result.
- The nested `bump` function demonstrates reading and updating an outer local.

## 3. Classes, methods, fields, and dynamic attributes

Runnable file: [examples/example3.pips](../examples/example3.pips)

```pips
class Pair {
  var left = 1;
  var right = 2;

  fn sum() {
    return left + right;
  }

  fn scale(k) {
    left = left * k;
    right = right * k;
    return this;
  }
}

var p = new Pair {
  .left = 10;
  .right = 20;
}

print(p.left);
p.right = 21;
print(p.sum());
print(p.scale(2).right);

setattr(p, "label", "pair");
print(getattr(p, "label"));
print(hasattr(p, "left"), hasattr(p, "missing"));
print(str(p.right));
```

This example covers:

- Script-scope class declarations
- Default field values
- `new Class { .field = expr; }` initialization
- Field reads and writes
- Method calls and `this`
- Dynamic attributes with `setattr`, `getattr`, and `hasattr`

## 4. Vectors, indexing, and slicing

Runnable file: [examples/example4.pips](../examples/example4.pips)

```pips
var v = [10, 20, 30, 40];
var w = [1, 2, 3, 4];

print(v);
print(v[0] + w[3]);

v[1] = 99;
print(v[1]);

print(v[1:3]);
print(v[:2]);
print(v[2:]);

v[1:3] = [7, 8];
print(v);
```

## Suggested order

If you are learning the language from scratch, run the examples in this order:

1. Expressions and built-ins
2. Control flow and functions
3. Classes and dynamic attributes
4. Vectors and slicing
