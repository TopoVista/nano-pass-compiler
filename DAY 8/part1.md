Day 8 – Language Design: Language Features and Grammar

We must first decide on the core features of our toy language LambdaCalc++ (inspired by C++ but simplified for a nanopass compiler).

 We include the following constructs: variable declarations (using a let keyword), arithmetic and comparison expressions, conditional statements (if/else), loops (while), functions (with parameters and calls), and a built-in print statement. 
 
 These cover the essentials of a C-like imperative language at an intermediate level.
 
  Each construct will have a corresponding non-terminal in our grammar. For example, many languages use a var or let rule for declarations (e.g. Lox’s grammar has varDecl → "var" Identifier ( "=" expression )? ";"
craftinginterpreters.com
), and use rules for if, while, print, etc. We similarly include rules like ifStmt → "if" "(" expression ")" statement [ "else" statement ] and printStmt → "print" expression ";"
craftinginterpreters.com
, adapting them to our syntax. (Our language uses C-style braces { } for blocks and semicolons, but uses let for declarations, for example.)

To formalize the syntax, we write an Extended Backus–Naur Form (EBNF) grammar. EBNF is a common notation for context-free grammars, extending basic BNF with operators for repetition and optional elements
tomassetti.me
tomassetti.me
. In EBNF we use | for choice, { ... } for zero-or-more repetition, and [ ... ] for optional parts. Below is the complete EBNF grammar for LambdaCalc++, capturing all the above features. This grammar is inspired by examples in language-design texts
maxgcoding.com
craftinginterpreters.com
, but adapted to use a let keyword and C-like syntax:
```
Program       ::= { FunctionDecl | Statement }
Statement     ::= LetStmt 
                | IfStmt 
                | WhileStmt 
                | PrintStmt 
                | ExprStmt

LetStmt       ::= "let" Identifier "=" Expression ";"
IfStmt        ::= "if" "(" Expression ")" Block [ "else" Block ]
WhileStmt     ::= "while" "(" Expression ")" Block
PrintStmt     ::= "print" Expression ";"
ExprStmt      ::= Expression ";"

FunctionDecl  ::= "function" Identifier "(" [ ParamList ] ")" Block
ParamList     ::= Identifier { "," Identifier }

Block         ::= "{" { Statement } "}"

Expression    ::= EqualityExpr
EqualityExpr  ::= RelationalExpr { ("==" | "!=") RelationalExpr }
RelationalExpr ::= AddExpr { ("<" | ">" | "<=" | ">=") AddExpr }
AddExpr       ::= MulExpr { ("+" | "-") MulExpr }
MulExpr       ::= UnaryExpr { ("*" | "/") UnaryExpr }
UnaryExpr     ::= "-" UnaryExpr | Primary

Primary       ::= Number 
                | Identifier 
                | FunctionCall 
                | "(" Expression ")"
FunctionCall  ::= Identifier "(" [ ArgList ] ")"
ArgList       ::= Expression { "," Expression }

Number        ::= Digit { Digit } [ "." Digit { Digit } ]
Identifier    ::= Letter { Letter | Digit }
Digit         ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
Letter        ::= "a" | ... | "z" | "A" | ... | "Z" | "_"

```
Explanation:

Program is a sequence of declarations or statements. We allow top-level functions (FunctionDecl) as well as ordinary statements.

LetStmt handles variable declaration (let x = Expr;). We treat let as a keyword (analogous to C++ or JavaScript) and require an initializer.

IfStmt and WhileStmt use C-like syntax with parentheses for the condition and a { } block for the body. The else branch on an if is made optional with [...].

PrintStmt is a simple print-output statement (modeled after many toy languages) that prints an expression’s value. The grammar rule printStmt → "print" expression ";"
craftinginterpreters.com
 guided this design.

FunctionDecl defines functions: we use the keyword "function", an identifier, a parenthesized parameter list (possibly empty), and a body block. This follows patterns like funDecl → "fun" function and function → IDENTIFIER "(" parameters? ")" block in existing grammars
craftinginterpreters.com
craftinginterpreters.com
, but adapted to our function keyword. Parameters are comma-separated identifiers.

Expression covers arithmetic and comparisons. We break it into precedence levels: multiplication/division (*//), addition/subtraction (+/-), and then relational (<, >, <=, >=) and equality (==, !=) comparisons. This lets us parse expressions like x + y * (z - 1) >= 5 == false, etc. Parentheses are handled by the rule Primary ::= "(" Expression ")".

FunctionCall allows calling functions in expressions (e.g. f(a, b+1)). It is parsed whenever an identifier is followed by ( ... ) in a primary expression. We allow zero or more comma-separated arguments (ArgList).

Number and Identifier are lexical tokens: numbers are integer or decimal literals, and identifiers start with a letter or underscore and continue with letters/digits. (These rules use regex-like notation; actual lexer rules would implement the same patterns.)

This complete EBNF grammar specifies the syntax of LambdaCalc++. It is written in the usual EBNF style (the most common formalism for grammar definitions
tomassetti.me
) and covers all required features. Similar grammars in textbooks and tutorials use the same constructs: for example, a teaching grammar includes if, while, print, etc. as alternatives in the Statement rule
maxgcoding.com
craftinginterpreters.com
, and handles repetition/optional parts with { } and [ ] as above.

Sources: We based our approach on standard examples of language grammars. EBNF is explained as the standard metalanguage for grammars
tomassetti.me
. Sample toy-language grammars (e.g. from Crafting Interpreters and other tutorials) illustrate similar rules for variables, if/else, loops, functions, and print
maxgcoding.com
craftinginterpreters.com
craftinginterpreters.com
. These informed our feature selection and grammar structure.