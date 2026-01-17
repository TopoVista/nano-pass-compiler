#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cctype>      // for isdigit, isalpha, isalnum
#include <stdexcept>   // for runtime_error
#include <memory>
#include "../ast/stmt.h"
#include "../ast/expr.h"
#include "../lexer/lexer.h"
#include "../lexer/token.h"



using namespace std;


class Parser{
    vector<Token> tokens;
    int current=0;
public:
    Parser(vector<Token> t):tokens(move(t)){}

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

    bool check(TokenType type) {
        if (isAtEnd()) return false;
        return peek().type == type;
    }


    /* ================= STATEMENTS ================= */

    unique_ptr<Stmt> statement(){
        if(match({TokenType::IF}))    return ifStatement();
        if(match({TokenType::WHILE})) return whileStatement();   // ⭐ NEW
        if(match({TokenType::FOR}))   return forStatement();
        if(match({TokenType::PRINT})) return printStatement();
        if(match({TokenType::LBRACE}))return blockStatement();
        if(match({TokenType::FUNCTION})) return functionStatement();
        if(match({TokenType::RETURN}))   return returnStatement();
        return expressionStatement();
    }

    unique_ptr<Stmt> whileStatement(){
        consume(TokenType::LPAREN,"Expected '(' after while");
        auto condition = expression();
        consume(TokenType::RPAREN,"Expected ')' after condition");
        auto body = statement();
        auto e = make_unique<WhileStmt>(move(condition), move(body));
        e->loc = { previous().line, previous().col };
        return e;
    }

    unique_ptr<Stmt> forStatement() {
        consume(TokenType::LPAREN, "Expected '(' after 'for'");

        // init
        unique_ptr<Stmt> init = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            auto initExpr = expression();
            consume(TokenType::SEMICOLON, "Expected ';' after for initializer");
            init = make_unique<ExprStmt>(move(initExpr));
        } else {
            consume(TokenType::SEMICOLON, "Expected ';'");
        }

    // condition
        unique_ptr<Expr> condition = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            condition = expression();
        }
        consume(TokenType::SEMICOLON, "Expected ';' after loop condition");

    // increment (expression ONLY, no semicolon)
        unique_ptr<Expr> increment = nullptr;
        if (!check(TokenType::RPAREN)) {
            increment = expression();
        }

        consume(TokenType::RPAREN, "Expected ')' after for clauses");

        auto body = statement();

        auto e = make_unique<ForStmt>(
                move(init),
                move(condition),
                move(increment),
                move(body)
        );
        e->loc = { previous().line, previous().col };
        return e;
    }


    unique_ptr<Stmt> ifStatement(){
        consume(TokenType::LPAREN,"Expected '(' after if");
        auto condition=expression();
        consume(TokenType::RPAREN,"Expected ')' after condition");

        auto thenBranch=statement();
        unique_ptr<Stmt> elseBranch=nullptr;

        if(match({TokenType::ELSE}))
            elseBranch=statement();

        auto e = make_unique<IfStmt>(
            move(condition),
            move(thenBranch),
            move(elseBranch)
        );
        e->loc = { previous().line, previous().col };
        return e;
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
        auto e = make_unique<PrintStmt>(move(value));
        e->loc = { previous().line, previous().col };
        return e;
    }

    unique_ptr<Stmt> expressionStatement(){
        auto expr=expression();
        consume(TokenType::SEMICOLON,"Expected ';' after expression");
        return make_unique<ExprStmt>(move(expr));
    }

    //added on day 13
    unique_ptr<Stmt> functionStatement() {
        Token name=peek();
        consume(TokenType::IDENTIFIER,"Expected function name");

        consume(TokenType::LPAREN,"Expected '(' after function name");
        vector<string> params;

        if(!match({TokenType::RPAREN})) {
            do {
                Token p=peek();
                consume(TokenType::IDENTIFIER,"Expected parameter name");
            	params.push_back(p.lexeme);
            } while(match({TokenType::COMMA}));
            consume(TokenType::RPAREN,"Expected ')'");
        }

        consume(TokenType::LBRACE,"Expected '{' before function body");
        auto body=blockStatement();
        return make_unique<FunctionStmt>(
            name.lexeme,
            move(params),
            unique_ptr<BlockStmt>(static_cast<BlockStmt*>(body.release()))
        );
    }

    unique_ptr<Stmt> returnStatement(){
        unique_ptr<Expr> value=nullptr;
        if(!match({TokenType::SEMICOLON})){
            value=expression();
            consume(TokenType::SEMICOLON,"Expected ';' after return");
        }
        auto e = make_unique<ReturnStmt>(move(value));
        e->loc = { previous().line, previous().col };
        return e;
    }


    /* ================= EXPRESSIONS (UNCHANGED) ================= */

    unique_ptr<Expr> expression(){ return assignment(); }

    unique_ptr<Expr> equality(){
        auto expr=comparison();
        while(match({TokenType::EQUAL_EQUAL,TokenType::BANG_EQUAL})){
            string op=previous().lexeme;
            auto right=comparison();
            auto e = make_unique<BinaryExpr>(op,move(expr),move(right));
            e->loc = { previous().line, previous().col };
            expr = move(e);
        }
        return expr;
    }

    unique_ptr<Expr> comparison(){
        auto expr=term();
        while(match({TokenType::LESS,TokenType::LESS_EQUAL,
                    TokenType::GREATER,TokenType::GREATER_EQUAL})){
            string op=previous().lexeme;
            auto right=term();
            auto e = make_unique<BinaryExpr>(op,move(expr),move(right));
            e->loc = { previous().line, previous().col };
            expr = move(e);
        }
        return expr;
    }

    unique_ptr<Expr> term(){
        auto expr=factor();
        while(match({TokenType::PLUS,TokenType::MINUS})){
            string op=previous().lexeme;
            auto right=factor();
            auto e = make_unique<BinaryExpr>(op,move(expr),move(right));
            e->loc = { previous().line, previous().col };
            expr = move(e);
        }
        return expr;
    }

    unique_ptr<Expr> factor(){
        auto expr=unary();
        while(match({
            TokenType::STAR,
            TokenType::SLASH,
            TokenType::MOD
        })) {
            string op = previous().lexeme;
            auto right = unary();
            auto e = make_unique<BinaryExpr>(op, move(expr), move(right));
            e->loc = { previous().line, previous().col };
            expr = move(e);
        }
        return expr;
    }

    unique_ptr<Expr> unary(){
        if(match({TokenType::BANG,TokenType::MINUS})){
            string op=previous().lexeme;
            auto right=unary();
            auto e = make_unique<UnaryExpr>(op,move(right));
            e->loc = { previous().line, previous().col };
            return e;
        }
        return primary();
    }

    unique_ptr<Expr> primary() {
        if (match({TokenType::STRING})) {
            return make_unique<StringExpr>(previous().lexeme);
        }
        if(match({TokenType::NUMBER})) {
            auto e = make_unique<NumberExpr>(stod(previous().lexeme));
            e->loc = { previous().line, previous().col };
            return e;
        }
        if(match({TokenType::IDENTIFIER})) {
        	string name=previous().lexeme;

        	if(match({TokenType::LPAREN})) {
                vector<unique_ptr<Expr>> args;
                if(!match({TokenType::RPAREN})) {
                    do {
                    	args.push_back(expression());
                	} while(match({TokenType::COMMA}));
                	consume(TokenType::RPAREN,"Expected ')'");
                }
                auto e = make_unique<CallExpr>(name,move(args));
                e->loc = { previous().line, previous().col };
                return e;
            }

            auto e = make_unique<VariableExpr>(name);
            e->loc = { previous().line, previous().col };
            return e;
        }
        if(match({TokenType::LPAREN})) {
        	auto expr=expression();
        	consume(TokenType::RPAREN,"Expected ')'");
        	return expr;
        }
        throw runtime_error("Expected expression");
    }

    unique_ptr<Expr> assignment(){
        auto expr = equality();

        if(match({TokenType::EQUAL})){
            string op = previous().lexeme;
            auto value = assignment();

            auto var = dynamic_cast<VariableExpr*>(expr.get());
            if(!var)
                throw runtime_error("Invalid assignment target");

            auto e = make_unique<BinaryExpr>(
                op,
                move(expr),
                move(value)
            );
            e->loc = { previous().line, previous().col };
            return e;
        }

        return expr;
    }

};
