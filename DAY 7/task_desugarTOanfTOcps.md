Perfect — this will tie everything you’ve been learning together.
Let’s do it **manually**, step by step — starting with a small but non-trivial **toy program** and walking it through:

1. **Desugaring** (eliminate syntactic sugar → pure core)
2. **A-Normalization (ANF)** (name all intermediate results)
3. **CPS Transformation** (explicit continuations)

---

## 🎯 Toy Program (source)

Let’s start with a compact Scheme-style program:

```scheme
(let ((x (+ 2 3)))
  (* x 4))
```

This means:
“Compute `2 + 3`, bind it to `x`, then multiply `x` by `4`.”

---

## 🧩 Step 1: Desugaring (remove syntactic sugar)

Desugaring expands high-level forms like `let`, `begin`, `when`, etc. into pure lambda + application form.

### Expand `let`:

A `let` is just syntactic sugar for calling an anonymous lambda:

```
(let ((x e1)) e2)
↦ ((λ (x) e2) e1)
```

So:

```scheme
(let ((x (+ 2 3)))
  (* x 4))
```

becomes:

```scheme
((λ (x) (* x 4)) (+ 2 3))
```

✅ **Desugared form (core λ-calculus):**

```scheme
((λ (x) (* x 4)) (+ 2 3))
```

Now, the program only uses lambdas, variables, primitives, and applications — no surface sugar.

---

## ⚙️ Step 2: A-Normalization (ANF)

**A-Normal Form (ANF)** ensures that *every non-atomic expression* is named by a `let`.
An *atomic* expression is: a variable, a literal, or a lambda.

Goal:

* No nested calls.
* Every function or argument position is atomic (variable or constant).

We start from:

```scheme
((λ (x) (* x 4)) (+ 2 3))
```

Let’s walk through it.

---

### Step 2.1 — identify non-atomic sub-expressions

* `(+ 2 3)` → non-atomic (a call)
* `(* x 4)` → non-atomic (a call)
* `(λ (x) (* x 4))` → **atomic** (lambda is atomic)

---

### Step 2.2 — introduce `let`s for each non-atomic

1. Introduce a name for `(+ 2 3)`
   Let `t1 = (+ 2 3)`
2. Inside the lambda, `(* x 4)` can also be made atomic with a `let`.

Result:

```scheme
(let ((t1 (+ 2 3)))
  ((λ (x)
     (let ((t2 (* x 4)))
       t2))
   t1))
```

✅ **ANF form complete**

* Every function position is atomic (`λ` or variable).
* Every argument is atomic (`t1`, `x`, or literal).

---

## 🔁 Step 3: CPS Transformation

Now we make **continuations explicit**.
Every function receives an *extra argument* — the continuation `k` — describing what to do next.

Rules (simplified hybrid style):

| Expression          | CPS Translation Idea                                                                                   |
| ------------------- | ------------------------------------------------------------------------------------------------------ |
| Constant `c`        | `(k c)`                                                                                                |
| Variable `x`        | `(k x)`                                                                                                |
| `(e1 e2)`           | transform both `e1` and `e2`; pass results to a function that applies the call and gives result to `k` |
| `(λ (x) body)`      | becomes `(λ (x k) <body-in-CPS>)`                                                                      |
| `(let ((x e1)) e2)` | transform `e1` in CPS, with continuation that binds `x` and continues with `e2`’s CPS                  |

Let’s walk it out.

---

### Step 3.1 — start from ANF

```scheme
(let ((t1 (+ 2 3)))
  ((λ (x)
     (let ((t2 (* x 4)))
       t2))
   t1))
```

Add a top-level continuation `halt` to catch the final result.

---

### Step 3.2 — CPS for the inner body `(let ((t2 (* x 4))) t2)`

Transform `(let ((t2 (* x 4))) t2)` in CPS form:

* `(* x 4)` is non-atomic; compute it, then continue.
* Let’s call the continuation for `(* x 4)` `k1`.

```scheme
(T-c (let ((t2 (* x 4))) t2) k)
```

→

```
(T-c (* x 4) (λ (t2) (k t2)))
```

→ expand call:

```
(T-c (* x 4) (λ (t2) (k t2)))
→ (T-k * (λ (f)
     (T*-k (x 4) (λ (args)
       `(,f ,@args ,(λ (t2) (k t2)))))))
```

But we’ll write it intuitively:

✅ **CPS for body of λ:**

```scheme
(λ (x k)
   (* x 4 (λ (t2)
            (k t2))))
```

---

### Step 3.3 — CPS for the outer `let`

Outer ANF:

```scheme
(let ((t1 (+ 2 3)))
  ((λ (x) body) t1))
```

In CPS:

1. Transform `(+ 2 3)` and then call the λ with that result.
2. The top continuation is `halt`.

So:

```
(+ 2 3 (λ (t1)
         ((λ (x k)
             (* x 4 (λ (t2)
                      (k t2))))
          t1
          halt)))
```

---

✅ **Final CPS form (conceptual simplified CPS)**

```scheme
(+ 2 3
   (λ (t1)
     ((λ (x k)
          (* x 4 (λ (t2)
                   (k t2))))
      t1
      halt)))
```

This is fully CPS’d:

* No implicit returns.
* Every computation is in tail position.
* Continuations (`λ (t1) ...`, `λ (t2) ...`, and `halt`) control the flow.

---

## 🔍 Step 4: Verify execution order

Let’s trace:

1. Evaluate `(+ 2 3)`
   → call continuation `(λ (t1) …)` with result 5
2. Inside that continuation, call `(λ (x k) …)` with `x=5`, `k=halt`
3. Inside function body:

   * Evaluate `(* x 4)`
     → `(λ (t2) (k t2))` gets `t2=20`
4. Finally `(k t2)` → `(halt 20)`

So the result is passed to `halt`, consistent with the original program result of `20`.

---

# ✅ Final Summary

| Stage         | Program                                                        |
| ------------- | -------------------------------------------------------------- |
| **Original**  | `(let ((x (+ 2 3))) (* x 4))`                                  |
| **Desugared** | `((λ (x) (* x 4)) (+ 2 3))`                                    |
| **ANF**       | `(let ((t1 (+ 2 3))) ((λ (x) (let ((t2 (* x 4))) t2)) t1))`    |
| **CPS**       | `(+ 2 3 (λ (t1) ((λ (x k) (* x 4 (λ (t2) (k t2)))) t1 halt)))` |

---

Would you like to do **one more example with `if` and `begin`**, so you see how control flow transforms through all three phases? That’s usually the next step after this basic one.
