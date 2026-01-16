# Partitioned CPS — presented cleanly

Below is a focused, self-contained explanation of the **Partitioned CPS** material you provided, organized for clarity: grammar, motivation, transform code shapes, stack recovery, scaling to real language features, and the hygiene caution for `letrec`.

---

# 1. Goal and intuition

CPS appears to remove the stack because **all calls become tail calls**. That *seems* to destroy the stack.
Partitioned CPS re-introduces a distinction between **user-level** computations and **continuation-level** computations so the stack can be recovered (and even implemented using the real machine stack). The trick: **tag** lambdas/variables/calls as user or continuation and keep separate grammars for each.

---

# 2. Partitioned grammar (what gets tagged)

Split expressions into user expressions (`uexp`), continuation expressions (`kexp`) and `call`s:

```
<uexp> ::= (λ (<uvar> <kvar>) <call>)   ; user lambda takes a user var and a continuation var
        |  <uvar>

<kexp> ::= (κ (<uvar>) <call>)           ; continuation (κ) takes a user var
        |  <kvar>

<call>  ::= ucall | kcall

<ucall> ::= (<uexp> <uexp> <kexp>)       ; user call: func user-arg continuation
<kcall> ::= (<kexp> <uexp>)              ; continuation call: continuation user-arg
```

* A `κ` form is like `λ` but explicitly marks a *continuation*.
* User lambdas take both a user argument and a continuation argument.
* Continuations take only a user argument.

---

# 3. Fresh names: separate namespaces

To keep user and continuation variables separate the transform uses two name generators:

* `genusym` → fresh user-bound variables (`$rv` etc.)
* `genksym` → fresh continuation-bound variables (`$k` etc.)

This prevents accidental mixing of user and continuation names and clarifies which stack slot each name should occupy.

---

# 4. Hybrid-style partitioned transform (shape)

There are three cooperating transformers in partitioned CPS:

* `T-k : expr × (aexp => cexp) => cexp` — higher-order style, receives a functional continuation builder `k`.
* `T-c : expr × aexp => cexp` — syntactic continuation style, receives an *atomic* continuation `c`.
* `M   : expr => aexp` — atomizer: converts lambdas & atomic expressions to CPS atoms.

Key differences vs earlier hybrid: `T-k` / `T-c` now produce `κ` and follow the partitioned grammar using `genusym` / `genksym`.

Example shapes (Racket-like):

```racket
(define (T-k expr k)
  (match expr
    [`(λ . ,_)      (k (M expr))]
    [ (? symbol?)   (k (M expr))]
    [`(,f ,e)
      (define $rv (genusym '$rv))
      (define cont `(κ (,$rv) ,(k $rv)))
      (T-k f (λ ($f)
               (T-k e (λ ($e)
                        `(,$f ,$e ,cont)))))]))
```

```racket
(define (T-c expr c)
  (match expr
    [`(λ . ,_)     `(,c ,(M expr))]
    [ (? symbol?)  `(,c ,(M expr))]
    [`(,f ,e)
      (T-k f (λ ($f)
               (T-k e (λ ($e)
                        `(,$f ,$e ,c)))))]))
```

```racket
(define (M expr)
  (match expr
    [`(λ (,var) ,expr)
      (define $k (genksym '$k))
      `(λ (,var ,$k) ,(T-c expr $k))]
    [(? symbol?) expr]))
```

* `T-k` builds a `κ` continuation `cont` using `genusym` for the continuation argument `$rv`.
* `M` creates user lambdas that accept a continuation (using `genksym` for `$k`) and transforms the body with `T-c`.

**Result (example):**

`(T-c '(g a) 'halt)` → `(g a halt)` — the ideal atomic call form.

---

# 5. Recovering the stack

Because continuations are **allocated in a last-allocated, first-invoked** order (LIFO):

* Allocate a continuation → bump stack pointer (reserve a stack frame).
* Invoke a continuation → restore stack pointer to that continuation’s stack frame (deallocating frames allocated after it).

Thus, even though CPS makes calls tail calls, you can **implement continuations as actual stack frames** and reuse the hardware stack pointer register to manage them. This is provably safe in the absence of `call/cc`, and the note claims the same safety extends under `call/ec` in the described treatment.

---

# 6. Scaling to real language features

The lambda calculus is a clean testbed, but the transform extends to real language constructs. Expand the source language to include:

**Source aexpr (atomic expressions)**

```
(λ (<var>*) <expr>)
<var>
#t | #f | <number> | <string> | (void)
call/ec | call/cc
```

**Source expr**

```
<aexpr>
(begin <expr>*)
(if <expr> <expr> <expr>)
(set! <var> <expr>)
(letrec ([<var> <aexpr>]*) <expr>)
(<prim> <expr>*)
(<expr> <expr>*)
```

`<prim>` ∈ `{+, -, /, *, =}`

**Target (expanded) CPS** — atomic and complex forms include conditionals, set-then!, letrec, primitive CPS ops and call forms:

```
<aexp> ::= (λ (<var>*) <cexp>) | <var> | #t | #f | <number> | <string> | (void)

<cexp> ::= (if <aexp> <cexp> <cexp>)
        |  (set-then! <var> <aexp> <cexp>)
        |  (letrec ([<var> <aexp>]*) <cexp>)
        |  ((cps <prim>) <aexp>*)
        |  (<aexp> <aexp>*)
```

---

# 7. The full-transform functions (four principals)

When scaling up, the transform typically uses:

* `T-c : expr × aexp => cexp`         — syntactic continuation entry
* `T-k : expr × (aexp => cexp) => cexp` — higher-order style for subexpressions
* `T*-k : expr* × (aexp* => cexp) => cexp` — transforms **lists** of expressions, returning list of atoms
* `M : expr => aexp`                  — atomizer

`T-c` acquires new cases for each language construct; e.g.:

* `begin` sequences: tail-call into last, use `T-k` for intermediate expressions.
* `if`: bind the continuation to avoid code blow-up, transform the condition with `T-k`, and produce an `if` that calls `T-c` for each branch with the same continuation.
* `set!`: transform RHS with `T-k`, then produce `(set-then! var aexp (c (void)))`.
* `letrec`: convert each binding expression with `M` and rebuild `letrec` in CPS form, then `T-c` the body.
* primitives: transform argument list with `T*-k` and emit `((cps <prim>) args... c)`.
* general call: `T-k` the function and `T*-k` the argument list, then emit `(<f> args... c)`.

Example `T-c` excerpt (Racket-like):

```racket
(define (T-c expr c)
  (match expr
    [ (? aexpr?)  `(,c ,(M expr))]                ; atomic
    [`(begin ,expr)      (T-c expr c)]
    [`(begin ,expr ,exprs ...)
      (T-k expr (λ (_) (T-c `(begin ,@exprs) c)))]
    [`(if ,exprc ,exprt ,exprf)
      (define $k (gensym '$k))
      `((λ (,$k)
          ,(T-k exprc (λ (aexp)
                        `(if ,aexp 
                             ,(T-c exprt $k)
                             ,(T-c exprf $k)))))
        ,c)]
    [`(set! ,var ,expr)
      (T-k expr (λ (aexp)
                  `(set-then! ,var ,aexp `(,c (void)))))]

    [`(letrec ([,vs ,as] ...) ,expr)
     `(letrec (,@(map list vs (map M as))) ,(T-c expr c))]

    [`(,(and p (? prim?)) ,es ...)  ; primitive
      (T*-k es (λ ($es)
                 `((cps ,p) ,@$es ,c)))]
    [`(,f ,es ...)                  ; general application
      (T-k f (λ ($f)
               (T*-k es (λ ($es)
                          `(,$f ,@$es ,c)))))]))
```

`T*-k` (helper to atomize expression lists):

```racket
(define (T*-k exprs k)
  (cond
    [(null? exprs)   (k '())]
    [(pair? exprs)   (T-k (car exprs) (λ (hd)
                       (T*-k (cdr exprs) (λ (tl)
                         (k (cons hd tl))))))]))
```

---

# 8. call/cc and call/ec

These can be desugared into CPS-friendly forms. A typical desugaring for `call/cc` or `call/ec`:

```
(λ (f cc) (f (λ (x _) (cc x)) cc))
```

* This creates a function that hands `f` a procedure that captures the current continuation `cc` (wrapped to ignore the continuation argument of the captured value).

---

# 9. Hygiene warning (letrec)

The transform for `letrec` as written **is not hygienic**:

* The transformer can accidentally **capture continuation variables** or accidentally introduce the letrec'd bindings into the scope of the continuation `c`.
* Practical remedies:

  * **Alphabetize** (α-rename) the program first so no name clashes occur, or
  * Modify the `letrec` lowering so it binds the continuation **outside** the scope of the letrec bindings.

In short: **alphatize programs before transforming**, or carefully manage the scope of continuation variables when lowering `letrec`.

---

# 10. Key takeaways (concise)

* **Partitioned CPS** distinguishes *user* vs *continuation* code, enabling recovery of an actual stack despite CPS converting calls to tail position.
* Use **tagged grammars** (`λ` vs `κ`, ucall vs kcall) and **separate gensym** routines for user and continuation names.
* The transforms `T-c`, `T-k`, `T*-k`, and `M` cooperate; `T-c` handles syntactic continuations at outer levels, `T-k` handles higher-order continuation-building for subexpressions, and `T*-k` atomizes lists of expressions.
* The approach **scales** to booleans, numbers, strings, `begin`, `if`, `set!`, `letrec`, primitives and control operators like `call/cc`.
* **Recovering the stack:** allocate continuations on the stack in LIFO order — bump stack pointer at allocation, restore it on invocation; safe without `call/cc` and handled carefully when `call/cc`/`call/ec` appear.
* **Hygiene caution:** alpha-rename before transforming or scope continuations outside `letrec` to avoid capture.

---

If you want, I can next:

* produce a **compact reference** (one-page cheat-sheet) with the transform pseudo-code and grammar, or
* show a **step-by-step transformation** of a slightly larger example (with `if`, `begin`, and `letrec`) into partitioned CPS.

(You said you wanted only this content taught — I stopped here.)
