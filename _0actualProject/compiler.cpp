#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cctype>      // for isdigit, isalpha, isalnum
#include <stdexcept>   // for runtime_error
#include <memory>

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

// USES TokenType and Token FROM YOUR LEXER

struct Expr{
    virtual ~Expr()=default;
    virtual void print(int d=0)=0;
};

struct NumberExpr:Expr{
    double value;
    NumberExpr(double v):value(v){}
    void print(int d){
        cout<<string(d,' ')<<"Number("<<value<<")\n";
    }
};

struct VariableExpr:Expr{
    string name;
    VariableExpr(string n):name(move(n)){}
    void print(int d){
        cout<<string(d,' ')<<"Var("<<name<<")\n";
    }
};

struct UnaryExpr:Expr{
    string op;
    unique_ptr<Expr> right;
    UnaryExpr(string o,unique_ptr<Expr> r):op(move(o)),right(move(r)){}
    void print(int d){
        cout<<string(d,' ')<<"Unary("<<op<<")\n";
        right->print(d+2);
    }
};

struct BinaryExpr:Expr{
    string op;
    unique_ptr<Expr> left,right;
    BinaryExpr(string o,unique_ptr<Expr> l,unique_ptr<Expr> r):op(move(o)),left(move(l)),right(move(r)){}
    void print(int d){
        cout<<string(d,' ')<<"Binary("<<op<<")\n";
        left->print(d+2);
        right->print(d+2);
    }
};

/* ===================== STATEMENTS ===================== */

struct Stmt{
    virtual ~Stmt()=default;
    virtual void print(int d)=0;
};

struct ExprStmt:Stmt{
    unique_ptr<Expr> e;
    ExprStmt(unique_ptr<Expr>x):e(move(x)){}
    void print(int d){
        cout<<string(d,' ')<<"ExprStmt\n";
        e->print(d+2);
    }
};

struct PrintStmt:Stmt{
    unique_ptr<Expr> e;
    PrintStmt(unique_ptr<Expr>x):e(move(x)){}
    void print(int d){
        cout<<string(d,' ')<<"PrintStmt\n";
        e->print(d+2);
    }
};

struct BlockStmt:Stmt{
    vector<unique_ptr<Stmt>> stmts;
    void print(int d){
        cout<<string(d,' ')<<"Block\n";
        for(auto& s:stmts) s->print(d+2);
    }
};

struct IfStmt:Stmt{
    unique_ptr<Expr> cond;
    unique_ptr<Stmt> thenB, elseB;
    IfStmt(unique_ptr<Expr>c,unique_ptr<Stmt>t,unique_ptr<Stmt>e)
        :cond(move(c)),thenB(move(t)),elseB(move(e)){}
    void print(int d){
        cout<<string(d,' ')<<"If\n";
        cond->print(d+2);
        thenB->print(d+2);
        if(elseB){
            cout<<string(d,' ')<<"Else\n";
            elseB->print(d+2);
        }
    }
};
class Parser{
    vector<Token> tokens;
    int current=0;
public:
    Parser(vector<Token> t):tokens(move(t)){}

    // ===== NEW ENTRY POINT =====
    vector<unique_ptr<Stmt>> parseProgram(){
        vector<unique_ptr<Stmt>> program;
        while(!isAtEnd()){
            program.push_back(statement());
        }
        return program;
    }

private:
    Token& peek(){return tokens[current];}
    Token& previous(){return tokens[current-1];}
    bool isAtEnd(){return peek().type==TokenType::END_OF_FILE;}

    bool match(initializer_list<TokenType> types){
        for(auto t:types){
            if(peek().type==t){
                current++;
                return true;
            }
        }
        return false;
    }

    void consume(TokenType type,const string& msg){
        if(peek().type==type){
            current++;
            return;
        }
        throw runtime_error(msg);
    }

    /* ================= STATEMENTS ================= */

    unique_ptr<Stmt> statement(){
        if(match({TokenType::IF})) return ifStatement();
        if(match({TokenType::PRINT})) return printStatement();
        if(match({TokenType::LBRACE})) return blockStatement();
        return expressionStatement();
    }

    unique_ptr<Stmt> ifStatement(){
        consume(TokenType::LPAREN,"Expected '(' after if");
        auto condition=expression();
        consume(TokenType::RPAREN,"Expected ')' after condition");

        auto thenBranch=statement();
        unique_ptr<Stmt> elseBranch=nullptr;

        if(match({TokenType::ELSE})){
            elseBranch=statement();
        }

        return make_unique<IfStmt>(
            move(condition),
            move(thenBranch),
            move(elseBranch)
        );
    }

    unique_ptr<Stmt> blockStatement(){
        auto block=make_unique<BlockStmt>();
        while(!match({TokenType::RBRACE})){
            block->stmts.push_back(statement());
        }
        return block;
    }

    unique_ptr<Stmt> printStatement(){
        auto value=expression();
        consume(TokenType::SEMICOLON,"Expected ';' after print");
        return make_unique<PrintStmt>(move(value));
    }

    unique_ptr<Stmt> expressionStatement(){
        auto expr=expression();
        consume(TokenType::SEMICOLON,"Expected ';' after expression");
        return make_unique<ExprStmt>(move(expr));
    }

    /* ================= EXPRESSIONS (UNCHANGED) ================= */

    unique_ptr<Expr> expression(){
        return equality();
    }

    unique_ptr<Expr> equality(){
        auto expr=comparison();
        while(match({TokenType::EQUAL_EQUAL,TokenType::BANG_EQUAL})){
            string op=previous().lexeme;
            auto right=comparison();
            expr=make_unique<BinaryExpr>(op,move(expr),move(right));
        }
        return expr;
    }

    unique_ptr<Expr> comparison(){
        auto expr=term();
        while(match({TokenType::LESS,TokenType::LESS_EQUAL,
                     TokenType::GREATER,TokenType::GREATER_EQUAL})){
            string op=previous().lexeme;
            auto right=term();
            expr=make_unique<BinaryExpr>(op,move(expr),move(right));
        }
        return expr;
    }

    unique_ptr<Expr> term(){
        auto expr=factor();
        while(match({TokenType::PLUS,TokenType::MINUS})){
            string op=previous().lexeme;
            auto right=factor();
            expr=make_unique<BinaryExpr>(op,move(expr),move(right));
        }
        return expr;
    }

    unique_ptr<Expr> factor(){
        auto expr=unary();
        while(match({TokenType::STAR,TokenType::SLASH})){
            string op=previous().lexeme;
            auto right=unary();
            expr=make_unique<BinaryExpr>(op,move(expr),move(right));
        }
        return expr;
    }

    unique_ptr<Expr> unary(){
        if(match({TokenType::BANG,TokenType::MINUS})){
            string op=previous().lexeme;
            auto right=unary();
            return make_unique<UnaryExpr>(op,move(right));
        }
        return primary();
    }

    unique_ptr<Expr> primary(){
        if(match({TokenType::NUMBER})){
            return make_unique<NumberExpr>(stod(previous().lexeme));
        }
        if(match({TokenType::IDENTIFIER})){
            return make_unique<VariableExpr>(previous().lexeme);
        }
        if(match({TokenType::LPAREN})){
            auto expr=expression();
            consume(TokenType::RPAREN,"Expected ')' after expression");
            return expr;
        }
        throw runtime_error("Expected expression");
    }
};


// ================== TEST DRIVER ==================
int main(){
    string src =
        "if (x + 1 > 10) {"
        "   print x;"
        "} else {"
        "   print x + 1;"
        "}";

    Lexer lx(src);
    Parser p(lx.scanTokens());
    auto program = p.parseProgram();

    for(auto& s:program)
        s->print(0);
}

