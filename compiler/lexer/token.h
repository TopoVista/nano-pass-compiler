#pragma once
#include <string>

/*
===========================================
TOKEN TYPES
===========================================
Each token represents a category of lexeme.
Lexer converts raw characters into these tokens.
*/

enum class TokenType {

  // Literals
  NUMBER,
  IDENTIFIER,
  STRING,
  TRUE,
  FALSE,

  // Keywords
  LET,
  FUNCTION,
  IF,
  ELSE,
  WHILE,
  FOR,
  PRINT,
  RETURN,

  // Type keywords
  INT,
  FLOAT,
  DOUBLE,
  SHORT,
  LONG,
  UNSIGNED,
  CHAR,
  BOOL,
  VOID,

  // Operators
  PLUS,
  MINUS,
  STAR,
  SLASH,
  MOD,

  EQUAL,
  EQUAL_EQUAL,

  BANG,
  BANG_EQUAL,

  LESS,
  LESS_EQUAL,

  GREATER,
  GREATER_EQUAL,

  AND_AND,
  OR_OR,

  // Punctuation
  COLON,
  SEMICOLON,
  COMMA,

  LPAREN,
  RPAREN,

  LBRACE,
  RBRACE,

  LBRACKET,
  RBRACKET,

  END_OF_FILE
};

/*
===========================================
TOKEN STRUCTURE
===========================================
Each token stores:
- type   : what kind of token it is
- lexeme : actual text from source code
- line   : line number (for error reporting)
- col    : column number
*/

struct Token {
  TokenType type;
  std::string lexeme;
  int line;
  int col;
};
