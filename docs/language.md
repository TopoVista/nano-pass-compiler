# NanoPass language reference

NanoPass is a small, typed imperative language designed for transparent
compiler experiments.  It offers two source spellings that intentionally lower
to the same AST and carry the same semantics:

- **Compatibility syntax** keeps existing C-like programs valid.
- **NanoPass surface syntax** is the project’s named, type-forward dialect.

There is no hidden source rewrite and no second implementation pipeline. A
program written with either spelling reaches the same resolver, type checker,
CoreIR, ANF, CFG, SSA, optimizer, interpreter, and optional LLVM backend.

## NanoPass surface syntax

```nano
fn relax(distance: i32, edge: i32) -> i32 {
  let candidate: i32 = distance + edge;
  when (candidate < 1000) {
    emit candidate;
    yield candidate;
  } otherwise {
    yield 1000;
  }
}

fn main() -> i32 {
  let i: i32 = 0;
  loop (i < 3) {
    i = i + 1;
  }
  yield i;
}
```

### Keywords

| Keyword | Purpose | Compatibility spelling |
|---|---|---|
| `fn` | Function declaration | `return_type name(type name)` |
| `let` | Name-first typed binding | `type name` |
| `when` | Conditional branch | `if` |
| `otherwise` | Alternative branch | `else` |
| `loop` | Condition-controlled loop | `while` |
| `emit` | Print a value | `print` |
| `yield` | Return from a function | `return` |
| `halt` | Leave the innermost loop | `break` |
| `next` | Start the next loop iteration | `continue` |

`function` is accepted as a long form of `fn` for now. The canonical short
form is preferred in new NanoPass examples.

### Types

| NanoPass | Compatibility | Notes |
|---|---|---|
| `i32` | `int` | Signed 32-bit integer |
| `truth` | `bool` | Boolean value |
| `unit` | `void` | No result |
| `i32[N]` | `int[N]` | Fixed-size array |

The complete existing scalar type set (`float`, `double`, `char`, and others
where supported by the parser/type checker) remains available through its
compatibility spelling. `i32`, `truth`, and `unit` are aliases, not new runtime
types.

### Declarations and functions

```nano
let score: i32 = 42;
let distances: i32[6];

fn choose(left: i32, right: i32) -> i32 {
  when (left > right) { yield left; }
  yield right;
}
```

The function result arrow is required in the NanoPass surface form. Parameters
are name-first (`name: type`); fixed-size array types use the same type suffix
as local bindings.

## Compatibility syntax

Existing code stays valid:

```nano
int choose(int left, int right) {
  if (left > right) { return left; }
  return right;
}

int main() {
  int score = choose(4, 7);
  print(score);
  return score;
}
```

## Expressions and control flow

Both source forms support numeric arithmetic, comparisons, assignment,
short-circuit `&&`/`||`, unary `!`, function calls, fixed-array indexing,
conditionals, `while`/`loop`, and C-style `for` loops. Semicolons and braces
remain explicit so every control-flow construct has an unambiguous structured
shape for lowering.

The bundled **NanoPass Dijkstra** lab example demonstrates the surface dialect
with fixed arrays, a matrix graph, greedy selection, relaxation, nested loops,
and observable numeric results.

It exercises the explicit SSA route, so inspect or run it with
`--backend=ssa` (for example, `nano run examples/nanopass_dijkstra.nano
--backend=ssa -O2`). The compatibility AST LLVM backend is still intentionally
limited for programs with explicit loop-control statements; that is a backend
limitation, not a difference between the two source spellings.

## Current limitations

- Arrays have a fixed declared size; dynamic allocation is not implemented.
- `truth` is not implicitly assignable to `i32`.
- Anonymous block scopes are not available; declarations are scoped by the
  current language model rather than arbitrary brace nesting.
- The syntax aliases do not alter integer overflow, evaluation order, or any
  other language semantics.
