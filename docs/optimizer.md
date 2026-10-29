# Nano SSA optimizer

NanoPass optimizes verified SSA for inspection; it does not yet lower optimized
SSA into LLVM. Use `nano optimize examples/optimizer.nano -O2` or
`nano emit examples/optimizer.nano --format=optimized`.

## Pass architecture and verification

Each `SSAFunctionPass` receives only an `SSAFunction` plus a small context for
options and remarks. The optimization pipeline invokes `verifySSA` after every
pass. A malformed result is an internal compiler error that identifies both
the pass and function; it is never silently passed to a later optimization.

The effect model is conservative: constants, copies, unary, binary, and phi
operations are pure; indexed loads read memory; indexed/local stores write
memory; calls and local loads have unknown effects. DCE may remove only unused
pure values. CSE only considers pure binary operations.

## Pipelines

- **O0**: verification only.
- **O1**: constant folding, algebraic simplification, copy propagation,
  trivial-phi elimination, DCE.
- **O2**: SCCP, CFG cleanup, O1 simplification, CSE, DCE, cleanup.
- **O3**: O2 repeated to a bounded four-iteration fixed point; it stops early
  when no pass changes the function. It does not claim LICM, inlining, or an
  industrial-strength O3.

Custom pipelines use `--passes=sccp,dce`. Pass IDs are `constant-fold`,
`algebraic-simplify`, `sccp`, `copy-prop`, `trivial-phi`, `cse`,
`simplify-cfg`, and `dce`.

## Local simplification

Constant folding supports integer and floating arithmetic, comparisons,
boolean logic, unary negation, and logical-not. Division or modulo by zero is
left untouched. Algebraic simplification is intentionally limited to identities
such as `x + 0`, `x - 0`, `x * 1`, and `x / 1`; it never uses `x * 0`.

Trivial phi elimination ignores self-incomings and replaces a phi only when all
other incoming values agree. Copy propagation replaces all SSA use sites then
erases the copy. DCE iterates because deleting a dead instruction can expose
another dead definition.

## SCCP and CFG cleanup

SCCP uses an `Unknown / Constant / Overdefined` lattice. Executable edges are
tracked independently from value state; a phi meets only incoming values whose
predecessor edge is executable. When a branch condition becomes constant, SCCP
records the fact in the branch operand while preserving CFG consistency;
`simplify-cfg` turns it into a jump, recomputes predecessors, removes
unreachable blocks, and drops stale phi incomings.

For example, `branch true, bb1, bb2` becomes `jump bb1`; `bb2` is then
unreachable. A `print` in `bb2` can disappear only because the block itself is
proven unreachable—not because DCE considers `print` pure.

## CSE and observability

CSE value-numbers binary expressions by opcode, type, and operands. It
canonicalizes only commutative operations and requires the retained definition
to dominate the replacement. Thus matching expressions in sibling branches
are deliberately not merged. `nano optimize --show-before-after --remarks`
prints deterministic SSA and real counters such as folded instructions,
executable blocks, branches simplified, and instructions removed.

Each pass also records elapsed microseconds and before/after instruction,
block, and SSA-value counts. These are structural compiler telemetry, not
runtime performance claims. `simplify-cfg` additionally forwards only an
empty non-entry jump block with exactly one predecessor, and only if rewriting
successor phi predecessors cannot create a duplicate phi edge.

## Interprocedural CFG inlining

At O2 and above, direct calls can be expanded by the verifier-gated SSA CFG
inliner before function-local cleanup. It supports multi-block callees,
branches, loop backedges, internal phis, value-returning and void multi-return
paths. Each inline operation splits the caller around the call and rebuilds
edges from terminators; it does not rely on cleanup passes to repair invalid
SSA. Recursive SCC members, oversized callees, exhausted growth budgets,
non-returning callees, and malformed call shapes are rejected with structured
remarks. Cloned nested calls remain calls during the same inliner invocation.
