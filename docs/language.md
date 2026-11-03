# NanoPass language reference

NanoPass is a small typed imperative language designed for transparent compiler
experiments. Its primary surface is deliberately **C-forward**: type-first
declarations, function signatures, braces, semicolons, and control flow all
look familiar to C and C++ programmers. NanoPass’s signature comes from its
compact canonical type names and a rigorously inspectable compilation pipeline,
not from novelty syntax for its own sake.

There is one AST and one deterministic compiler pipeline. The C-forward form,
the original `int`/`bool` spelling, and optional alias keywords all lower into
the same resolver, type checker, CoreIR, ANF, CFG, SSA, optimizer, interpreter,
and optional LLVM backend.

## Canonical NanoPass C surface

```nano
i32 relax(i32 distance, i32 edge) {
  i32 candidate = distance + edge;
  if (candidate < 1000) {
    print(candidate);
    return candidate;
  } else {
    return 1000;
  }
}

i32 main() {
  i32 i = 0;
  while (i < 3) {
    i = i + 1;
  }
  return i;
}
```

### Canonical types

| NanoPass C | Existing alias | Meaning |
|---|---|---|
| `i32` | `int` | Signed 32-bit integer |
| `truth` | `bool` | Boolean value |
| `unit` | `void` | No result |
| `i32 values[N]` | `int[N] values` | Fixed-size array |

`i32 values[N]` uses C’s familiar declarator position. The older prefix-array
form remains accepted so existing examples do not change behavior. The scalar
types `float`, `double`, and `char` retain their existing spelling.

### Declarations, functions, and control flow

```nano
i32 score = 42;
i32 distances[6];
truth visited[6];

i32 choose(i32 left, i32 right) {
  if (left > right) { return left; }
  return right;
}

while (score > 0) {
  print(score);
  score = score - 1;
}
```

NanoPass supports arithmetic, comparisons, assignment, short-circuit `&&` and
`||`, unary `!`, direct function calls, fixed-array indexing, `if`/`else`,
`while`, C-style `for`, `break`, `continue`, `print`, and `return`.

## Optional aliases

The following aliases are accepted for experimentation and remain fully
backwards-compatible. They are not the canonical style used in new examples:

| Alias | Canonical NanoPass C spelling |
|---|---|
| `fn name(arg: i32) -> i32` | `i32 name(i32 arg)` |
| `let name: i32 = value;` | `i32 name = value;` |
| `when` / `otherwise` | `if` / `else` |
| `loop` | `while` |
| `emit` | `print` |
| `yield` | `return` |
| `halt` / `next` | `break` / `continue` |

Aliases do not affect overflow behavior, evaluation order, optimization
legality, or generated code. They only choose a source spelling.

## Dijkstra example

The bundled [NanoPass C Dijkstra example](../examples/nanopass_dijkstra.nano)
uses fixed arrays, an adjacency matrix, greedy selection, relaxation, nested
loops, and observable results. Inspect or run it through the explicit SSA path:

```sh
nano verify examples/nanopass_dijkstra.nano --backend=ssa -O2
nano run examples/nanopass_dijkstra.nano --backend=ssa -O2
```

The compatibility AST LLVM backend remains intentionally limited for programs
with explicit loop-control statements. That is a backend limitation, not a
difference between the supported source spellings.

## Current limitations

- Arrays have a fixed declared size; dynamic allocation is not implemented.
- `truth` is not implicitly assignable to `i32`.
- Anonymous block scopes are not available; declarations follow the existing
  language scope model rather than arbitrary brace nesting.
