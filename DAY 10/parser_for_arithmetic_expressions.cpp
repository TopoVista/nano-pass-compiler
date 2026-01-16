#include <bits/stdc++.h>
using namespace std;

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

class Parser{
    vector<Token> tokens;
    int current=0;
public:
    Parser(vector<Token> t):tokens(move(t)){}

    unique_ptr<Expr> parse(){
        return expression();
    }

private:
    Token& peek(){return tokens[current];}
    Token& previous(){return tokens[current-1];}
    bool isAtEnd(){return peek().type==TokenType::END_OF_FILE;}

    bool match(initializer_list<TokenType> types){
        for(auto t:types){
            if(peek().type==t){current++;return true;}
        }
        return false;
    }

    void consume(TokenType type,const string& msg){
        if(peek().type==type){current++;return;}
        throw runtime_error(msg);
    }

    // GRAMMAR IMPLEMENTATION
    // expression → equality
    unique_ptr<Expr> expression(){
        return equality();
    }

    // equality → comparison ( (== | !=) comparison )*
    unique_ptr<Expr> equality(){
        auto expr=comparison();
        while(match({TokenType::EQUAL_EQUAL,TokenType::BANG_EQUAL})){
            string op=previous().lexeme;
            auto right=comparison();
            expr=make_unique<BinaryExpr>(op,move(expr),move(right));
        }
        return expr;
    }

    // comparison → term ( (< | <= | > | >=) term )*
    unique_ptr<Expr> comparison(){
        auto expr=term();
        while(match({TokenType::LESS,TokenType::LESS_EQUAL,TokenType::GREATER,TokenType::GREATER_EQUAL})){
            string op=previous().lexeme;
            auto right=term();
            expr=make_unique<BinaryExpr>(op,move(expr),move(right));
        }
        return expr;
    }

    // term → factor ( (+ | -) factor )*
    unique_ptr<Expr> term(){
        auto expr=factor();
        while(match({TokenType::PLUS,TokenType::MINUS})){
            string op=previous().lexeme;
            auto right=factor();
            expr=make_unique<BinaryExpr>(op,move(expr),move(right));
        }
        return expr;
    }

    // factor → unary ( (* | /) unary )*
    unique_ptr<Expr> factor(){
        auto expr=unary();
        while(match({TokenType::STAR,TokenType::SLASH})){
            string op=previous().lexeme;
            auto right=unary();
            expr=make_unique<BinaryExpr>(op,move(expr),move(right));
        }
        return expr;
    }

    // unary → (! | -) unary | primary
    unique_ptr<Expr> unary(){
        if(match({TokenType::BANG,TokenType::MINUS})){
            string op=previous().lexeme;
            auto right=unary();
            return make_unique<UnaryExpr>(op,move(right));
        }
        return primary();
    }

    // primary → NUMBER | IDENTIFIER | '(' expression ')'
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
    string src="x + 3 * (y - 2) <= 10 == !z";
    Lexer lx(src);
    vector<Token> toks=lx.scanTokens();
    Parser p(toks);
    auto ast=p.parse();
    ast->print();
}
