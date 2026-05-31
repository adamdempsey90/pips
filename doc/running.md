# Running Code

See also [Language guide](language.md) and [Examples](examples.md).

## Modes

The standalone executable supports three practical ways to run code.

### Interactive REPL

Start it with:

```bash
./repl/repl
```

Behavior:

- A line ending in `;` runs immediately.
- Multi-line input stays buffered until you enter a blank line.
- `clear` clears the screen.
- `exit` leaves the REPL.
- `!command` runs a shell command.

That makes the REPL convenient for quick expressions and for typing multi-line blocks such as functions, classes, `if`, `while`, and `for`.

### Run a file

Use `-i` to execute a script file:

```bash
./repl/repl -i path/to/input.pips
```

Files are parsed in statement mode, where simple statements normally end with `;`.

Examples:

```pips
var x = 1;
print(x);
```

A trailing `;` is not required after a class body, a function body, or a `new Class { ... }` initializer when the statement naturally ends at `}`.

Examples:

```pips
class Foo { var bar; }

fn add(a, b) {
  return a + b;
}

var f = new Foo {
  .bar = "value";
}
```

### Run a snippet from the shell

Use `-c` for short snippets:

```bash
./repl/repl -c 'var x = 2' 'print(x)'
```

Important details:

- Each shell argument after `-c` is treated as one source line.
- The launcher appends `;` to each of those arguments for you.
- Do not pack several semicolon-separated statements into one `-c` argument.
- Multi-line constructs work best when each logical line is passed as its own argument.

Example:

```bash
./repl/repl -c \
  'class Foo { var bar; }' \
  'var f = new Foo { .bar = "ok" }' \
  'print(f.bar)'
```

## Helpful flags

- `-v` prints the source being executed before it runs.
- `-r` runs any provided files or snippets and then drops into the REPL.

## Choosing between REPL and files

Use the REPL when you want quick feedback, shell escapes, and incremental typing.
Use files when you want repeatable scripts, longer examples, or anything you plan to save.
Use `-c` when you want shell-friendly one-offs without creating a file.

## ODE solver application

The `applications/` directory contains a standalone RK4 ODE solver that uses a
pips script to define the problem. Build it with the rest of the project:

```bash
mkdir build && cd build
cmake .. && make
```

The executable is `build/applications/ode_solver`. It takes exactly one argument:
the path to a pips script file.

```bash
./applications/ode_solver ../applications/sho.pips
```

Output is CSV written to stdout:

```
t,y0,y1
0,1,0
0.03141592653589793,0.9995065603670668,-0.03141075882311788
...
```

### Script contract

The script must define two globals and two functions before the solver reads
them. The names are fixed.

| Symbol | Kind | Meaning |
|---|---|---|
| `time_range` | vector | Sequence of time points; at least 2 elements. The solver steps from each `t[i]` to `t[i+1]` using `dt = t[i+1] - t[i]`, so the points do not need to be uniform. |
| `dimension` | number | Number of state variables. |
| `initial_condition()` | function | Returns a vector of length `dimension` with the initial state `y(t[0])`. |
| `rhs(t, y)` | function | Returns a vector of length `dimension` giving the time derivative `dy/dt` at time `t` and state `y`. |

A minimal script:

```pips
var time_range = linspace(0, 10, 1001);
var dimension  = 1;

fn initial_condition() {
    return [1.0];
}

fn rhs(t, y) {
    return [-y[0]];   # y' = -y  →  exact solution y = exp(-t)
}
```

The provided example scripts are:

- `applications/sho.pips` — simple harmonic oscillator (2-D; exact: `y0 = cos(t)`, `y1 = -sin(t)`)
- `applications/lorenz.pips` — Lorenz attractor (3-D; sigma=10, rho=28, beta=8/3)

### Redirecting output

Pipe or redirect stdout to process results:

```bash
./applications/ode_solver ../applications/lorenz.pips > lorenz.csv
```
