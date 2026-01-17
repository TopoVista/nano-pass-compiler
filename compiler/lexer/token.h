#pragma once
#include <string>

/*
===========================================
TOKEN TYPES
===========================================
Each token represents a *category* of lexeme.
Lexer converts raw characters into these tokens.
*/

enum class TokenType {
    NUMBER, IDENTIFIER,
    LET, FUNCTION, IF, ELSE, WHILE, FOR, PRINT, RETURN,
    PLUS, MINUS, STAR, SLASH, MOD,
    EQUAL, EQUAL_EQUAL,
    BANG, BANG_EQUAL,
    LESS, LESS_EQUAL,
    GREATER, GREATER_EQUAL,
    SEMICOLON, COMMA,
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    END_OF_FILE
};

/*
===========================================
TOKEN STRUCTURE
===========================================
Each token stores:
- type   : what kind of token it is
- lexeme: actual text from source code
- line   : line number (for error reporting)
- col    : column number
*/

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int col;
};
