#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cctype>      // for isdigit, isalpha, isalnum
#include <stdexcept>   // for runtime_error
#include <memory>
#include "../lexer/token.h"
#include "../sema/symbol.h"
#include "../sema/type.h"



using namespace std;


// USES TokenType and Token FROM YOUR LEXER

struct Expr {
    Type type = Type::Unknown();   // 👈 ADD THIS
    virtual ~Expr() = default;
    virtual void print(int d) = 0;
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
    Symbol* symbol = nullptr;

    VariableExpr(string n):name(move(n)){}
    
    void print(int d){
        cout<<string(d,' ')<<"Var("<<name;
        if(symbol)
            cout<<" -> depth "<<symbol->depth;
        cout<<")\n";
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

//added on day 13
struct CallExpr:Expr{
    string callee;
    vector<unique_ptr<Expr>> args;

    CallExpr(string c,vector<unique_ptr<Expr>> a)
        :callee(move(c)),args(move(a)){}
    
    void print(int d){
        cout<<string(d,' ')<<"Call "<<callee<<"\n";
        for(auto& a:args) a->print(d+2);
    }
};

