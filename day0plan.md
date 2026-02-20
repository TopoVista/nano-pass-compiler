Perfect. If you’re starting from **zero compiler design knowledge** (but with AFL/TOC background), I’ll lay out a **day-by-day nano-pass compiler roadmap** that assumes:

* You know **C++**, but not how compilers are built.
* You’re willing to read, code, and practice 5 hrs/day.
* You want a **nano-pass compiler as the final deliverable**.

This is an **8-week plan (~2 months, 5 hrs/day = ~300 hrs)**. Think of it like a **bootcamp syllabus**, but tailored just for you.

---

# 📅 Day-by-Day Ultra-Detailed Nano-Pass Compiler Roadmap

---

## **Week 1 – Foundations of Compilers + Nano-Pass Philosophy**

📖 Goal: Understand **what a compiler is**, why nano-pass matters, and basic building blocks.

**Day 1**

* Read: *Engineering a Compiler* (Cooper & Torczon), Ch. 1 (Introduction).
* Watch: What is a Compiler? — Computerphile.
* Task: Write a summary in your notes: *Phases of a compiler*.

**Day 2**

* Read: *Essentials of Programming Languages* (EOPL), Ch. 1 (Interpreters).
* Task: Implement a tiny interpreter in C++ for arithmetic (`1+2*3`).
* This builds AST intuition.

**Day 3**

* Read: EOPL, Ch. 2 (Syntactic Sugar).
* Task: Hand-desugar `for` loops → `while`, `x += y` → `x = x+y`.
* Write 3 examples on paper.

**Day 4**

* Read: Flatt & Felleisen, *The NanoPass Framework* (skim examples).
* Watch: [Matthew Flatt – NanoPass Compiler Design (talk)](https://www.youtube.com/watch?v=Vm4tx1O9GAc).
* Task: Write notes on **why many small passes are better**.

**Day 5**

* Learn **A-Normal Form (ANF)**: read short guide ([link](https://matt.might.net/articles/a-normalization/)).
* Task: Convert 3 expressions into ANF by hand.

**Day 6**

* Learn **Continuation-Passing Style (CPS)**: [Intro article](https://matt.might.net/articles/cps-conversion/).
* Task: Transform factorial function into CPS on paper.

**Day 7**

* Recap week: Write a “mini compiler pipeline by hand” for a small program, showing desugaring → ANF → CPS.
* Task: Document in notes.

---

## **Week 2 – Language Design + Parsing**

📖 Goal: Define your toy language (“LambdaCalc++”) and build the **front end** (lexer + parser).

**Day 8**

* Decide language features: `let`, arithmetic, if/else, while, functions, print.

* Write grammar in **EBNF**. Example:
  
  ```
  Expr ::= Number | Identifier | Expr "+" Expr | ...
  ```

**Day 9**

* Read: Stanford CS143 notes on Lexical Analysis.
* Task: Write a **lexer in C++** that tokenizes numbers, identifiers, operators.

**Day 10**

* Read: Recursive-Descent Parsing tutorial.
* Task: Implement parser for arithmetic expressions → AST.

**Day 11**

* Extend parser for `if/else`.
* Extend AST nodes (use `struct` + `std::unique_ptr`).

**Day 12**

* Extend parser for `while`.
* Task: Parse sample input into AST (print tree structure).

**Day 13**

* Add functions + calls to parser.
* Task: Parse factorial function → AST.

**Day 14**

* Recap week: parser demo — input program → AST dump.

---

## **Week 3 – First Nano-Passes (Surface → Core AST)**

📖 Goal: Implement **first transformations** (nano-passes).

**Day 15**

* Implement **Pass 1: Desugar `for` loops → while`.**
* Task: Run test code.

**Day 16**

* Implement **Pass 2: Desugar `+=` → assignment + addition.**
* Extend test suite.

**Day 17**

* Implement **Pass 3: Desugar `if/else` → primitive conditional form.**

**Day 18**

* Implement **Pass 4: Introduce temporaries (ANF).**

* Example:
  
  ```
  a+b*c → let t1 = b*c; let t2 = a+t1;
  ```

**Day 19**

* Visualize AST after each pass using Graphviz.
* Resource: Graphviz quickstart.

**Day 20**

* Write pretty-printer for IR after each pass.

**Day 21**

* Recap week: Run a program, dump AST after each pass.

---

## **Week 4 – Semantic Analysis + Type Checking**

📖 Goal: Add **symbol tables, scope, type checking**.

**Day 22**

* Read: Appel, *Modern Compiler Implementation*, Ch. 5 (symbol tables).
* Implement symbol table in C++ (`unordered_map`).

**Day 23**

* Pass: **Scope resolution** (replace identifiers with bindings).

**Day 24**

* Implement type checker: infer int/bool types.

**Day 25**

* Pass: Convert `true/false` into `1/0`.

**Day 26**

* Add semantic errors (undefined variable, type mismatch).

**Day 27**

* Improve error messages (line/column numbers).

**Day 28**

* Recap week: Run test programs with semantic analysis.

---

## **Week 5 – Lowering to CPS**

📖 Goal: Lower Core AST → CPS IR (resume gold).

**Day 29**

* Re-read CPS conversion guide.
* Task: Hand-convert factorial to CPS again.

**Day 30**

* Implement **Pass: Core AST → CPS IR**.

**Day 31**

* Write CPS IR printer (to text).

**Day 32**

* Run test programs through CPS lowering.

**Day 33–35**

* Debug, refine, test.

---

## **Week 6 – LLVM Backend**

📖 Goal: Convert IR → LLVM IR, generate executables.

**Day 36**

* Read LLVM Kaleidoscope Tutorial (Part 1).
* Write: minimal LLVM IR “return 42” in C++.

**Day 37**

* Implement **Pass: CPS IR → LLVM IR** (expressions).

**Day 38**

* Add support for if/else, while.

**Day 39**

* Add support for functions + calls.

**Day 40–42**

* Compile factorial, Fibonacci → LLVM IR → assembly → executable.

---

## **Week 7 – Optimizations as Nano-Passes**

📖 Goal: Add classic optimizations.

**Day 43**

* Implement Pass: Constant Folding.

**Day 44**

* Implement Pass: Dead Code Elimination.

**Day 45**

* Implement Pass: Inline simple functions.

**Day 46–47**

* Run optimizations in pipeline.

**Day 48–49**

* Visualize before/after optimizations in Graphviz.

**Day 50**

* Recap: Optimization pipeline.

---

##  **Week 8 – Polish + Demo**

📖 Goal: Prepare for resume + professor demo.

**Day 51–52**

* Write documentation:
  
  * Language grammar
  * Pass pipeline diagram
  * IR samples

**Day 53**

* Polish error handling (pretty error messages).

**Day 54–55**

* Write demo programs:
  
  * Factorial
  * Fibonacci
  * Sorting
  * Nested loops
  * Function recursion

**Day 56**

* Final run-through: source → all passes → LLVM executable.

**Day 57–58**

* Polish repo (README, install instructions).

**Day 59–60**

* Record demo (video / screenshots).
* Push final version to GitHub.

---

# 🎯 Deliverable

By Day 60, you’ll have:

* A custom language (“LambdaCalc++”).
* Compiler with **10–12 nano-passes**.
* Multi-IR pipeline: Surface → Core → ANF → CPS → LLVM IR.
* Optimizations as passes.
* Graphviz visualizations.
* Working executables from your language.
* Documentation + repo ready for resume.

---

🔥 This is resume gold — most undergrads stop at “tiny C compiler.” You’ll show **nano-pass design, multiple IRs, LLVM backend, optimizations, error handling**.

---

Do you want me to also **list all the exact books/lectures/blogs you’ll use on each day** (like a clickable reading pack), so you don’t waste time searching?

Great — let’s make this a **“compiler learning pack”**, where every single day has **one exact resource** (book chapter, blog, or video) and one coding/practice task. You won’t waste time searching — just follow the list.

---

# 📚 Day-by-Day Nano-Pass Compiler Resource Plan

---

## **Week 1 – Foundations + Nano-Pass Philosophy**

📖 Goal: Learn phases of compilers, nano-pass mindset, ANF, CPS.

* **Day 1**
  
  * 📘 *Engineering a Compiler* (Cooper & Torczon), Ch. 1.
  * 🎥 [What is a Compiler? — Computerphile](https://www.youtube.com/watch?v=CSZLNYF4Klo)
  * ✅ Task: Write compiler pipeline diagram in notes.

* **Day 2**
  
  * 📘 *Essentials of Programming Languages (EOPL)*, Ch. 1.
  * ✅ Task: Implement tiny interpreter in C++ (`1+2*3`).

* **Day 3**
  
  * 📘 *EOPL*, Ch. 2 (syntactic sugar).
  * ✅ Task: Desugar `for` → `while`, `+=` → `= +`.

* **Day 4**
  
  * 📄 [Flatt & Felleisen, *The NanoPass Framework*](https://nanopass.org/papers.html) (skim examples).
  * 🎥 [Matthew Flatt — NanoPass Compiler Design](https://www.youtube.com/watch?v=Vm4tx1O9GAc)
  * ✅ Task: Write “why nano-pass?” summary.

* **Day 5**
  
  * 📄 [A-Normalization (Matt Might blog)](https://matt.might.net/articles/a-normalization/)
  * ✅ Task: Convert 3 expressions to ANF.

* **Day 6**
  
  * 📄 [CPS Conversion (Matt Might blog)](https://matt.might.net/articles/cps-conversion/)
  * ✅ Task: Convert factorial to CPS.

* **Day 7**
  
  * ✅ Task: Manually transform a toy program through desugaring → ANF → CPS.

---

## **Week 2 – Language Design + Parsing**

📖 Goal: Design toy language & implement lexer/parser.

* **Day 8**
  
  * 📄 [EBNF Tutorial](https://www.w3schools.com/xml/schema_complex_ebnf.asp)
  * ✅ Task: Write grammar for your language (arithmetic, if, while, functions).

* **Day 9**
  
  * 📄 [Stanford CS143 – Lexical Analysis notes](https://web.stanford.edu/class/cs143/lectures/lecture02.pdf)
  * ✅ Task: Write C++ lexer (tokens: NUM, ID, +, -, etc.).

* **Day 10**
  
  * 📄 [Recursive Descent Parsing Tutorial (eli.thegreenplace.net)](https://eli.thegreenplace.net/2010/09/29/recursive-descent-parsing/)
  * ✅ Task: Parse arithmetic → AST.

* **Day 11**
  
  * 📘 *Engineering a Compiler*, Ch. 2 (Parsing basics).
  * ✅ Task: Extend parser with if/else.

* **Day 12**
  
  * ✅ Task: Extend parser with while loops.

* **Day 13**
  
  * ✅ Task: Extend parser with functions + calls.

* **Day 14**
  
  * ✅ Task: Dump AST of factorial function.

---

## **Week 3 – First Nano-Passes**

📖 Goal: Implement nano-passes that simplify AST.

* **Day 15**
  
  * ✅ Task: Pass 1 – Desugar `for` → while.

* **Day 16**
  
  * ✅ Task: Pass 2 – Desugar `+=` → `x = x + y`.

* **Day 17**
  
  * ✅ Task: Pass 3 – Normalize if/else → core conditional.

* **Day 18**
  
  * 📄 [A-Normalization guide again](https://matt.might.net/articles/a-normalization/)
  * ✅ Task: Implement ANF pass.

* **Day 19**
  
  * 📄 [Graphviz intro](https://graphviz.gitlab.io/documentation/)
  * ✅ Task: Dump AST after each pass with Graphviz.

* **Day 20**
  
  * ✅ Task: Write pretty-printer for IR.

* **Day 21**
  
  * ✅ Task: Run pipeline: source → AST → Passes → IR dump.

---

## **Week 4 – Semantic Analysis**

📖 Goal: Type-checker & scope handling.

* **Day 22**
  
  * 📘 Appel, *Modern Compiler Implementation*, Ch. 5.
  * ✅ Task: Build symbol table in C++.

* **Day 23**
  
  * ✅ Task: Pass – Scope resolution.

* **Day 24**
  
  * 📘 *Engineering a Compiler*, Ch. 4 (Type Systems).
  * ✅ Task: Implement int/bool type checker.

* **Day 25**
  
  * ✅ Task: Pass – Convert `true/false` → 1/0.

* **Day 26–27**
  
  * ✅ Task: Add semantic error handling (undefined var, type mismatch).

* **Day 28**
  
  * ✅ Task: Test programs with semantic errors.

---

## **Week 5 – CPS Lowering**

📖 Goal: CPS IR implementation.

* **Day 29**
  
  * 📄 [CPS conversion guide](https://matt.might.net/articles/cps-conversion/) (re-read).
  * ✅ Task: Hand-convert factorial again.

* **Day 30**
  
  * ✅ Task: Implement AST → CPS IR pass.

* **Day 31**
  
  * ✅ Task: Write CPS IR printer.

* **Day 32–35**
  
  * ✅ Task: Test with different programs.

---

## **Week 6 – LLVM Backend**

📖 Goal: Generate LLVM IR.

* **Day 36**
  
  * 📄 [LLVM Kaleidoscope Tutorial (Part 1)](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html)
  * ✅ Task: Emit `ret i32 42` in LLVM IR.

* **Day 37**
  
  * ✅ Task: CPS IR → LLVM IR (arithmetic).

* **Day 38**
  
  * ✅ Task: Add if/else + while.

* **Day 39**
  
  * ✅ Task: Add functions + calls.

* **Day 40–42**
  
  * ✅ Task: Compile factorial/fibonacci → LLVM → executable.

---

## **Week 7 – Optimizations**

📖 Goal: Classic nano-pass optimizations.

* **Day 43**
  
  * 📄 [Constant Folding (dragon book notes)](http://web.cs.ucla.edu/classes/spring10/cs132/lectures/notes09.pdf)
  * ✅ Task: Implement constant folding pass.

* **Day 44**
  
  * 📄 [Dead Code Elimination blog](https://mukulrathi.com/create-your-own-programming-language/llvm-ir-cpp-tutorial/)
  * ✅ Task: Implement DCE.

* **Day 45**
  
  * ✅ Task: Inline trivial functions.

* **Day 46–47**
  
  * ✅ Task: Run optimizer pipeline.

* **Day 48–49**
  
  * ✅ Task: Visualize before/after with Graphviz.

* **Day 50**
  
  * ✅ Task: Summarize optimization passes.

---

## **Week 8 – Polish + Demo**

📖 Goal: Make it “resume-ready”.

* **Day 51–52**
  
  * ✅ Task: Write grammar + pass pipeline documentation.

* **Day 53**
  
  * ✅ Task: Add pretty error messages.

* **Day 54–55**
  
  * ✅ Task: Write sample programs (factorial, fib, sorting).

* **Day 56**
  
  * ✅ Task: Full pipeline demo (source → binary).

* **Day 57–58**
  
  * ✅ Task: Polish repo (README, instructions).

* **Day 59–60**
  
  * ✅ Task: Record demo video + push to GitHub.

---

# 🎯 What you get

* 📂 GitHub repo: Nano-pass compiler in C++.
* 📄 Docs: Grammar, pipeline diagram, IR dumps, passes.
* ⚙️ Executables: Factorial, Fibonacci compiled by *your* compiler.
* ⭐ Resume bullet: *“Built nano-pass compiler from scratch with LLVM backend, 12+ passes, and optimization pipeline.”*

---

Would you like me to also **prepare a printable one-page “Nano-Pass Compiler Roadmap PDF”** with all resources + tasks neatly laid out, so you can keep it on your desk while working?

Perfect 👍 — I’ll give you the **exact chapter names** from the main books in your roadmap so you know what to read (and where to stop). I’ll only list the ones **relevant to building a Nano-Pass compiler** (not the entire book).

---

# 📘 **Essentials of Programming Languages (EOPL, 3rd Ed.)**

(Friedman, Wand, Haynes)

* **Ch. 1: Introduction**
* **Ch. 2: Inductive Sets of Data**
* **Ch. 3: Expressions**
* **Ch. 4: Functions**
* **Ch. 5: Recursion**
* **Ch. 6: Data Structures**
* **Ch. 7: Mutation**

👉 Stop here. This covers **interpreters, environments, closures, recursive functions, mutation** — the foundation for compiler passes. Later chapters are deep theory you won’t need for the project.

---

# 📘 **Engineering a Compiler (2nd Ed.)**

(Cooper & Torczon)

* **Ch. 1: Introduction**
* **Ch. 2: A Simple Syntax-Directed Translator**
* **Ch. 3: Lexical Analysis**
* **Ch. 4: Syntax Analysis**
* **Ch. 5: Semantic Analysis**
* **Ch. 6: Intermediate Representations**

👉 Stop after Ch. 6. You’ll have all compiler phases needed for a nano-pass design.

---

# 📘 **Modern Compiler Implementation in C**

(Appel)

* **Ch. 1: Introduction**
* **Ch. 2: Lexical Analysis**
* **Ch. 3: Parsing**
* **Ch. 4: Abstract Syntax**
* **Ch. 5: Semantic Analysis**
* **Ch. 6: Activation Records** (optional)
* **Ch. 7: Translation to Intermediate Code**

👉 This book is code-heavy. You’ll use it mostly for **lexing, parsing, ASTs, symbol tables**.

---

# 📘 **The NanoPass Framework: A Compiler for Teaching**

(Flatt & Felleisen, paper)

* Not a book, but a research paper.
  👉 Read the sections:
* **1. Introduction**
* **2. Motivation**
* **3. The NanoPass Framework**
* **4. Example Compiler**

That’s enough to get the **nano-pass mindset** (many tiny transformations, each very clear).

---

# 📘 **LLVM Kaleidoscope Tutorial (Official)**

* Part 1: Basic Language & Lexer
* Part 2: Adding a Parser
* Part 3: Code Generation to LLVM IR
* (Optional) Part 4+: Control Flow, Functions

---

✅ With just these chapters/sections, you’ll have **all the raw material to build your nano-pass compiler**.

---

Do you want me to now **map these chapters directly onto the day-by-day plan** I gave earlier? (e.g., “Day 1 → EOPL Ch.1 Sections 1.1–1.3”, “Day 2 → Engineering a Compiler Ch.2”, etc.) so you don’t have to think about scheduling?
