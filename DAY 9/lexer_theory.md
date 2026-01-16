Below is the **complete theory of Lexical Analysis** you should know **based strictly on this lexer**, from *foundations → formal theory → practical design decisions → edge cases → how this fits into a compiler*.
I’ll explicitly map **theory ↔ code** so you know *why each concept exists*, not just *what it does*.

---

# 1. What Lexical Analysis Is (Big Picture)

**Lexical Analysis (Scanning / Tokenization)** is the **first phase of a compiler**.

### Role in a compiler pipeline

```
Source Code (characters)
        ↓
LEXER  →  Tokens
        ↓
PARSER → AST
        ↓
Semantic Analysis
        ↓
IR / Codegen
```

Your lexer:

* Takes a **flat stream of characters**
* Groups them into **meaningful units (tokens)**
* Annotates tokens with **location info** (line, column)
* Removes **irrelevant characters** (whitespace, comments)

> **Key idea**:
> The lexer does *not* understand grammar or meaning — only **patterns of characters**.

---

# 2. Core Lexical Concepts (Must-Know Theory)

## 2.1 Character Stream

* Input is a **linear sequence of characters**
* No structure yet
* Your lexer treats it as:

```cpp
string src;
size_t current;
```

### Theoretical term:

➡ **Input alphabet (Σ)**
Example:

```
Σ = { letters, digits, symbols, whitespace }
```

---

## 2.2 Lexeme vs Token (CRITICAL)

### Lexeme

* The **actual text** in source code
  Examples:

```
"x", "42", "+", "while"
```

### Token

* A **classification** of lexemes
  Examples:

```
IDENTIFIER, NUMBER, PLUS, WHILE
```

### In your code:

```cpp
struct Token {
    TokenType type;   // category
    string lexeme;    // actual text
    int line, col;    // location
};
```

📌 **Theory rule**:

> Parser cares about **TokenType**, not raw text
> Error messages care about **lexeme + location**

---

## 3. Token Types & Language Design

Your `enum class TokenType` defines the **lexical vocabulary** of the language.

### Categories of tokens (theory):

| Category    | Examples             | Your Code          |
| ----------- | -------------------- | ------------------ |
| Literals    | numbers, identifiers | NUMBER, IDENTIFIER |
| Keywords    | reserved words       | LET, IF, WHILE     |
| Operators   | + - * /              | PLUS, MINUS        |
| Comparators | == <= >=             | EQUAL_EQUAL        |
| Separators  | ; ,                  | SEMICOLON          |
| Delimiters  | ( ) { }              | LPAREN             |
| Special     | end marker           | END_OF_FILE        |

📌 **Important theory**:

> Keywords are **identifiers with special meaning**

That’s why your lexer:

1. Scans identifiers
2. Then checks keyword table

```cpp
if (kw.contains(text))
    token = keyword
else
    token = identifier
```

---

# 4. Regular Languages & Lexical Rules

## 4.1 Lexical Analysis = Regular Language Recognition

Lexers are based on **Regular Languages**, described by:

* Regular Expressions
* DFAs / NFAs

Your lexer is a **hand-written DFA**, even if you didn’t explicitly draw one.

---

## 4.2 Implicit Regular Expressions in Your Code

### Identifiers

```
[a-zA-Z_][a-zA-Z0-9_]*
```

Code:

```cpp
isAlpha(c)
while (isAlphaNum(peek())) advance();
```

### Numbers

```
[0-9]+(\.[0-9]+)?
```

Code:

```cpp
while (isdigit(peek()))
if (peek()=='.' && isdigit(peekNext()))
```

### Operators

```
==, !=, <=, >=
```

Code:

```cpp
match('=')
```

📌 **Theory takeaway**:

> Every token kind corresponds to a **regular expression**

---

# 5. Finite State Machine View (VERY IMPORTANT)

Your lexer implements a **Deterministic Finite Automaton (DFA)**.

### States are implicit:

* Start state
* Number state
* Identifier state
* Operator state
* Comment state
* String state

Example for `/`:

```
/ → '/' → SLASH
     ↓
    '/' → COMMENT
     ↓
    '*' → MULTILINE COMMENT
```

Code:

```cpp
if (match('/')) { ... }
else if (match('*')) { ... }
else SLASH
```

📌 **Theory**:

> Handwritten lexers encode DFA transitions using conditionals

---

# 6. Greedy (Maximal Munch) Principle

### Rule:

> Always consume the **longest valid token**

Examples:

* `<=` not `<`
* `==` not `=`
* `123.45` not `123` `.` `45`

Your lexer obeys this by:

* Scanning until pattern breaks
* Using `match()` for lookahead

```cpp
match('=') ? EQUAL_EQUAL : EQUAL
```

📌 **This rule is foundational**
Violating it breaks parsing.

---

# 7. Lookahead & Backtracking

## 7.1 Lookahead

Your lexer uses **1-character lookahead**:

```cpp
peek()
peekNext()
```

This is called **LL(1) lexical lookahead**.

### Needed for:

* Decimal numbers
* Comments
* Compound operators

---

## 7.2 No Backtracking

Your lexer:

* Never rewinds `current`
* Only moves forward

📌 **Theory**:

> Efficient lexers are **single-pass, no backtracking**

---

# 8. Whitespace & Comment Handling

## 8.1 Whitespace Is Not a Token

Your lexer **ignores**:

* spaces
* tabs
* carriage returns
* newlines (except for line count)

📌 Theory:

> Whitespace is **lexically insignificant** in most languages

---

## 8.2 Comments Are Lexical Constructs

### Single-line comment:

```cpp
// until newline
```

### Multi-line comment:

```cpp
/* ... */
```

Handled entirely in lexer, **never seen by parser**.

📌 **Important**:

> Comments are NOT grammar constructs
> They must be removed during lexical analysis

---

# 9. String Literals (Lexical Pitfalls)

You currently treat strings as IDENTIFIER (placeholder).

### Theoretical issues with strings:

* Escape sequences (`\n`, `\"`)
* Multiline strings
* Unterminated strings
* Encoding (UTF-8)

Your check:

```cpp
if (isAtEnd()) throw error
```

📌 **Theory rule**:

> Lexers must detect *lexical errors*, not parsers

---

# 10. Error Handling in Lexers

Your lexer detects:

* Unexpected characters
* Unterminated strings

```cpp
throw runtime_error("Unexpected character")
```

### Theory:

Lexical errors include:

* Invalid characters
* Malformed numbers
* Unterminated literals

📌 **Key idea**:

> Lexer errors are *local* and *early*

---

# 11. Position Tracking (Line & Column)

You maintain:

```cpp
line++;
col++;
```

### Why this matters:

* Precise error messages
* IDE diagnostics
* Debugging compiler stages

📌 **Theory**:

> Tokens must carry source location metadata

---

# 12. End-of-File Token (EXTREMELY IMPORTANT)

```cpp
END_OF_FILE
```

### Purpose:

* Signals parser to stop
* Avoids bounds checks
* Simplifies grammar rules

📌 **Theory rule**:

> Every lexer must emit an explicit EOF token

---

# 13. Handwritten Lexer vs Generated Lexer

### Your approach:

* Handwritten
* Full control
* Easy to debug
* Ideal for compilers & interpreters

### Generated (lex/flex):

* Regex-based
* Faster to write
* Harder to customize

📌 **For nanopass compilers**:

> Handwritten lexer is the **correct choice**

---

# 14. Why Lexer Is Separate from Parser

| Lexer               | Parser                |
| ------------------- | --------------------- |
| Characters → Tokens | Tokens → AST          |
| Regular languages   | Context-free grammar  |
| DFA                 | Pushdown automaton    |
| Fast, simple        | Recursive, structured |

📌 **Golden rule**:

> Parser must NEVER inspect raw characters

---

# 15. Complexity Guarantees

Your lexer:

* **Time**: O(n)
* **Space**: O(n) tokens
* **Single pass**

📌 **Theory**:

> Lexical analysis is linear-time by design

---

# 16. What You Should Know NEXT (Theory Roadmap)

To go deeper, next theory topics should be:

1. Formal DFA construction from regex
2. Longest-match conflicts
3. Unicode & UTF-8 lexing
4. Indentation-sensitive lexing (Python)
5. Token buffering for parser
6. Error recovery strategies
7. Lexer modes (C preprocessor, JS template strings)

---

## Final Mental Model (Remember This)

> **Lexer = DFA that converts characters into tokens using greedy matching, lookahead, and regular-language rules, while discarding irrelevant input and preserving location metadata.**

If you want next:

* I can **convert this lexer into a formal DFA diagram**
* Or show **how this feeds a recursive-descent parser**
* Or show **how real compilers (Clang, Rust) do lexing**

Just tell me where you want to go next.
