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

    /* ================= STATEMENTS ================= */

    unique_ptr<Stmt> statement(){
        if(match({TokenType::IF}))    return ifStatement();
        if(match({TokenType::WHILE})) return whileStatement();   // ⭐ NEW
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
        return make_unique<WhileStmt>(move(condition), move(body));
    }

    unique_ptr<Stmt> ifStatement(){
        consume(TokenType::LPAREN,"Expected '(' after if");
        auto condition=expression();
        consume(TokenType::RPAREN,"Expected ')' after condition");

        auto thenBranch=statement();
        unique_ptr<Stmt> elseBranch=nullptr;

        if(match({TokenType::ELSE}))
            elseBranch=statement();

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
        return make_unique<ReturnStmt>(move(value));
    }


    /* ================= EXPRESSIONS (UNCHANGED) ================= */

    unique_ptr<Expr> expression(){ return equality(); }

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

    unique_ptr<Expr> primary() {
	    if(match({TokenType::NUMBER}))
		    return make_unique<NumberExpr>(stod(previous().lexeme));
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
			    return make_unique<CallExpr>(name,move(args));
		    }

		    return make_unique<VariableExpr>(name);
	    }
	    if(match({TokenType::LPAREN})) {
	    	auto expr=expression();
	    	consume(TokenType::RPAREN,"Expected ')'");
	    	return expr;
    	}
	    throw runtime_error("Expected expression");
    }
};
