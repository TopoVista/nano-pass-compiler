# Day 7 — Mini compiler pipeline: desugaring → ANF → CPS

**Goal:** Show a short, hand-transformed pipeline for a small program so you can see how desugaring, ANF, and CPS reshape code step-by-step.

---

## 0) Original small program (toy imperative language)

```pseudo
def foo(x) {
  return x + 1;
}

let r = 0;
for (i = 1; i <= 3; i += 1) {
  r += foo(i * 2);
}
print(r);
```

Notes about this source:

* `for` and `+=` are syntactic sugar that can be rewritten.
* `foo` is a simple function; `foo(i * 2)` contains a non-atomic argument (`i * 2`) that will matter for ANF.

---

## 1) Desugaring (surface syntax → simpler core)

Transform language-level sugar into a smaller core. Two straightforward rewrites:

* `for(init; cond; step) { body }` → `init; while (cond) { body; step }`
* `a += b` → `a = a + b`

**After desugaring:**

```pseudo
def foo(x) {
  return x + 1;
}

let r = 0;
i = 1;
while (i <= 3) {
  r = r + foo(i * 2);
  i = i + 1;
}
print(r);
```

Explanation: now control flow is built from simpler constructs (`while`, `=` and primitive operators). This reduces the number of special cases later.

---

## 2) A-Normal Form (ANF)

**ANF idea (quick):** all non-trivial expressions (function calls, binary ops, etc.) are named with a `let`-binding; function call arguments must be *atomic* (variables or constants).

We will convert expressions like `r = r + foo(i * 2)` into a sequence of small bindings.

**Transform step-by-step:**

* `i * 2` is non-atomic → bind: `t1 = i * 2`
* `foo(t1)` is a call → bind: `t2 = foo(t1)`
* `r + t2` is non-atomic → bind: `t3 = r + t2`
* `r = t3`

**Program in ANF:**

```pseudo
def foo(x) {
  t0 = x + 1
  return t0
}

let r = 0
i = 1
while (i <= 3) {
  t1 = i * 2
  t2 = foo(t1)
  t3 = r + t2
  r = t3
  i = i + 1
}
print(r)
```

Notes:

* Inside `foo`, the expression `x + 1` becomes `t0 = x + 1; return t0`.
* Every expression that is not an atomic literal or variable is now one `let` binding. This invariant simplifies reasoning and later transformations (e.g., register allocation, CPS conversion).

---

## 3) Continuation-Passing Style (CPS)

**CPS idea (quick):** every function is rewritten to take an extra parameter `k` (the continuation), and instead of returning a result it *calls* `k(result)`. Control flow is explicit: "what to do next" is a function.

We'll transform expressions and function definitions into CPS. To keep the example readable, we show a *direct-style* CPS that keeps the imperative `while` but makes all expression results passed to continuations.

### 3.1 `foo` → CPS

Original ANF `foo`:

```pseudo
def foo(x) {
  t0 = x + 1
  return t0
}
```

CPS form (foo_cps takes `x` and a continuation `k`):

```pseudo
def foo_cps(x,k) {
  t0 = x + 1
  k(t0)
}
```

There is no `return` anymore; we call the continuation with the computed value.

### 3.2 The main body in CPS

We convert each step so that instead of computing a value and assigning, we sequence via continuations. We'll define a top-level continuation `halt` which prints the final `r` and finishes.

**Strategy:** Convert each statement into a CPS-styled function that accepts a continuation representing "what to do next".

Sketch of the transformed program (read top-to-bottom):

```pseudo
def halt(r) {
  print(r)
}

# foo_cps defined earlier
def foo_cps(x,k) {
  t0 = x + 1
  k(t0)
}

# Entry: we start the computation by calling start_cps(halt)
def start_cps(k) {
  r = 0
  i = 1
  # we define a loop body as a recursive function in CPS style
  def loop_cps(r,i,k_loop) {
    if (i <= 3) {
      t1 = i * 2
      # call foo_cps with t1 and a continuation that receives t2
      foo_cps(t1, lambda t2 {
        t3 = r + t2
        r2 = t3
        i2 = i + 1
        loop_cps(r2,i2,k_loop)
      })
    } else {
      k_loop(r)  # loop finished, pass r to the continuation
    }
  }
  loop_cps(r,i,k)
}

# Kick off
start_cps(halt)
```

Explanation and mapping:

* `foo` became `foo_cps(x,k)` and calls `k(result)` instead of `return`.
* The `while` loop is expressed as a recursive function `loop_cps(r,i,k_loop)` which:

  * checks the loop condition `i <= 3`.
  * if true, computes `t1 = i * 2`, calls `foo_cps(t1, cont)` where `cont` continues the loop by updating `r` and `i` and recursing.
  * else, calls `k_loop(r)` to propagate the final `r` to the outer continuation.
* `start_cps(halt)` initializes `r` and `i` and begins the loop; when loop finishes it calls `halt(r)`, which prints.

**Why CPS is useful here:**

* Control flow (looping, early exits, exceptions, continuations) becomes explicit and manipulable as first-class functions.
* Optimizations like inlining, tail-call elimination, and non-local control flow (e.g., implementing `break` or coroutines) become easier.

---

## 4) Compact mapping summary

* **Source**: reusable syntactic sugar + high-level constructs (for, +=, return)
* **Desugared**: reduced to a small core (`while`, `=`, primitive ops)
* **ANF**: created explicit temporaries for non-atomic expressions; function-call arguments made atomic
* **CPS**: converted functions to take continuations; returns replaced by continuation calls; loop turned into recursive CPS loop

---

## 5) Final notes / checklist for your handwritten pipeline

* When doing this on paper, always:

  1. Mark each expression that is not an atom and give it a fresh temporary name for ANF.
  2. After ANF, ensure every function call and operator application has only variables/constants as operands.
  3. For CPS, add a continuation parameter to every function, and convert `return e` into `k(e_cps)` where `e_cps` is the (already ANF'd) value.
  4. Convert imperative loops to recursive CPS loops (or keep `while` but make local continuations that handle the body).

* Try the same pipeline on these small variants next: `(a) nested calls: f(g(h(x)))`, `(b) early-return in a function`, `(c) `for`with`break`/`continue` (shows how CPS makes non-local control explicit).

---

If you want, I can also produce a second worked example (pure expression-oriented) or show the full mechanical CPS transform rules used line-by-line.
