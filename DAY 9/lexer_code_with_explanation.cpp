#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cctype>      // for isdigit, isalpha, isalnum
#include <stdexcept>   // for runtime_error

using namespace std;

/*
===========================================
TOKEN TYPES
===========================================
Each token represents a *category* of lexeme.
Lexer converts raw characters into these tokens.
*/
enum class TokenType {
    NUMBER, IDENTIFIER,               // literals
    LET, FUNCTION, IF, ELSE, WHILE, PRINT, RETURN, // keywords
    PLUS, MINUS, STAR, SLASH,          // arithmetic operators
    EQUAL, EQUAL_EQUAL,                // = and ==
    BANG, BANG_EQUAL,                  // ! and !=
    LESS, LESS_EQUAL,                  // < and <=
    GREATER, GREATER_EQUAL,            // > and >=
    SEMICOLON, COMMA,                  // separators
    LPAREN, RPAREN,                    // ( )
    LBRACE, RBRACE,                    // { }
    END_OF_FILE                        // marks end of input
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
    string lexeme;
    int line;
    int col;
};

/*
===========================================
LEXER CLASS
===========================================
Responsible for converting source code string
into a list of tokens.
*/
class Lexer {
    string src;           // full source code
    size_t start = 0;     // beginning of current token
    size_t current = 0;   // current character index
    int line = 1;         // current line number
    int col = 1;          // current column number

    // map of keyword text -> token type
    unordered_map<string, TokenType> kw;

public:
    /*
    ----------------------------------------
    CONSTRUCTOR
    ----------------------------------------
    - Stores source code
    - Initializes keyword table
    */
    Lexer(string s) : src(move(s)) {
        kw["let"]      = TokenType::LET;
        kw["function"] = TokenType::FUNCTION;
        kw["if"]       = TokenType::IF;
        kw["else"]     = TokenType::ELSE;
        kw["while"]    = TokenType::WHILE;
        kw["print"]    = TokenType::PRINT;
        kw["return"]   = TokenType::RETURN;
    }

    /*
    ----------------------------------------
    scanTokens()
    ----------------------------------------
    Main driver function:
    - Walks through entire source code
    - Produces vector<Token>
    */
    vector<Token> scanTokens() {
        vector<Token> tokens;

        // keep scanning until end of input
        while (!isAtEnd()) {
            start = current;        // mark beginning of new token
            char c = advance();     // consume next character

            switch (c) {

            // ignore whitespace
            case ' ':
            case '\t':
            case '\r':
                break;

            // new line handling
            case '\n':
                line++;
                col = 1;
                break;

            // single-character tokens
            case '+': tokens.push_back(makeToken(TokenType::PLUS)); break;
            case '-': tokens.push_back(makeToken(TokenType::MINUS)); break;
            case '*': tokens.push_back(makeToken(TokenType::STAR)); break;

            /*
            --------------------------------
            SLASH
            --------------------------------
            Could be:
            - division operator
            - single-line comment //
            - multi-line comment /* */
            case '/':
                if (match('/')) {
                    // single-line comment → skip until newline
                    while (peek() != '\n' && !isAtEnd()) advance();
                }
                else if (match('*')) {
                    // multi-line comment
                    while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
                        if (peek() == '\n') {
                            line++;
                            col = 1;
                        }
                        advance();
                    }
                    if (!isAtEnd()) {
                        advance(); // *
                        advance(); // /
                    }
                }
                else {
                    tokens.push_back(makeToken(TokenType::SLASH));
                }
                break;

            // comparison and assignment operators
            case '=':
                tokens.push_back(makeToken(
                    match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL
                ));
                break;

            case '!':
                tokens.push_back(makeToken(
                    match('=') ? TokenType::BANG_EQUAL : TokenType::BANG
                ));
                break;

            case '<':
                tokens.push_back(makeToken(
                    match('=') ? TokenType::LESS_EQUAL : TokenType::LESS
                ));
                break;

            case '>':
                tokens.push_back(makeToken(
                    match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER
                ));
                break;

            // punctuation
            case ';': tokens.push_back(makeToken(TokenType::SEMICOLON)); break;
            case ',': tokens.push_back(makeToken(TokenType::COMMA)); break;
            case '(': tokens.push_back(makeToken(TokenType::LPAREN)); break;
            case ')': tokens.push_back(makeToken(TokenType::RPAREN)); break;
            case '{': tokens.push_back(makeToken(TokenType::LBRACE)); break;
            case '}': tokens.push_back(makeToken(TokenType::RBRACE)); break;

            /*
            --------------------------------
            STRING LITERALS
            --------------------------------
            NOTE: you currently classify them
            as IDENTIFIER (can be improved later)
            */
            case '"': {
                while (peek() != '"' && !isAtEnd()) {
                    if (peek() == '\n') {
                        line++;
                        col = 1;
                    }
                    advance();
                }
                if (isAtEnd())
                    throw runtime_error("Unterminated string");

                advance(); // consume closing quote

                string lex = src.substr(start + 1, current - start - 2);
                tokens.push_back(Token{TokenType::IDENTIFIER, lex, line, col});
                break;
            }

            default:
                if (isdigit(c)) {
                    number(tokens);
                }
                else if (isAlpha(c)) {
                    identifier(tokens);
                }
                else {
                    throw runtime_error(string("Unexpected character: ") + c);
                }
                break;
            }
        }

        // always append EOF token
        tokens.push_back(Token{TokenType::END_OF_FILE, "", line, col});
        return tokens;
    }

private:
    /*
    ----------------------------------------
    HELPER FUNCTIONS
    ----------------------------------------
    */

    bool isAtEnd() const {
        return current >= src.size();
    }

    // consume current character and move forward
    char advance() {
        char c = src[current++];
        col++;
        return c;
    }

    // look at current char without consuming
    char peek() const {
        return isAtEnd() ? '\0' : src[current];
    }

    // look one character ahead
    char peekNext() const {
        return current + 1 >= src.size() ? '\0' : src[current + 1];
    }

    // conditionally consume expected character
    bool match(char expected) {
        if (isAtEnd()) return false;
        if (src[current] != expected) return false;
        current++;
        col++;
        return true;
    }

    // create token from [start, current)
    Token makeToken(TokenType type) {
        string lex = src.substr(start, current - start);
        return Token{type, lex, line, col};
    }

    bool isAlpha(char c) const {
        return isalpha((unsigned char)c) || c == '_';
    }

    bool isAlphaNum(char c) const {
        return isalnum((unsigned char)c) || c == '_';
    }

    /*
    ----------------------------------------
    NUMBER SCANNING
    ----------------------------------------
    Handles integers and decimals
    */
    void number(vector<Token>& tokens) {
        while (isdigit(peek())) advance();

        // decimal part
        if (peek() == '.' && isdigit(peekNext())) {
            advance();
            while (isdigit(peek())) advance();
        }

        tokens.push_back(makeToken(TokenType::NUMBER));
    }

    /*
    ----------------------------------------
    IDENTIFIER / KEYWORD SCANNING
    ----------------------------------------
    */
    void identifier(vector<Token>& tokens) {
        while (isAlphaNum(peek())) advance();

        string text = src.substr(start, current - start);

        auto it = kw.find(text);
        if (it != kw.end())
            tokens.push_back(Token{it->second, text, line, col});
        else
            tokens.push_back(Token{TokenType::IDENTIFIER, text, line, col});
    }
};

/*
===========================================
TOKEN TYPE TO STRING (DEBUGGING)
===========================================
*/
string tokenTypeName(TokenType t) {
    switch (t) {
    case TokenType::NUMBER: return "NUMBER";
    case TokenType::IDENTIFIER: return "IDENTIFIER";
    case TokenType::LET: return "LET";
    case TokenType::FUNCTION: return "FUNCTION";
    case TokenType::IF: return "IF";
    case TokenType::ELSE: return "ELSE";
    case TokenType::WHILE: return "WHILE";
    case TokenType::PRINT: return "PRINT";
    case TokenType::RETURN: return "RETURN";
    case TokenType::PLUS: return "PLUS";
    case TokenType::MINUS: return "MINUS";
    case TokenType::STAR: return "STAR";
    case TokenType::SLASH: return "SLASH";
    case TokenType::EQUAL: return "EQUAL";
    case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
    case TokenType::BANG: return "BANG";
    case TokenType::BANG_EQUAL: return "BANG_EQUAL";
    case TokenType::LESS: return "LESS";
    case TokenType::LESS_EQUAL: return "LESS_EQUAL";
    case TokenType::GREATER: return "GREATER";
    case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
    case TokenType::SEMICOLON: return "SEMICOLON";
    case TokenType::COMMA: return "COMMA";
    case TokenType::LPAREN: return "LPAREN";
    case TokenType::RPAREN: return "RPAREN";
    case TokenType::LBRACE: return "LBRACE";
    case TokenType::RBRACE: return "RBRACE";
    case TokenType::END_OF_FILE: return "EOF";
    default: return "UNKNOWN";
    }
}

/*
===========================================
MAIN FUNCTION (TEST DRIVER)
===========================================
*/
int main() {
    string src =
        "let x = 42;"
        "print x + 3*(2+1);"
        "function fact(n){"
        "if(n<=1){return 1;}"
        "else{return n*fact(n-1);}"
        "}";

    Lexer lx(src);
    vector<Token> toks = lx.scanTokens();

    for (auto &t : toks)
        cout << t.line << ":" << t.col << "\t"
             << tokenTypeName(t.type) << "\t'"
             << t.lexeme << "'\n";
}
