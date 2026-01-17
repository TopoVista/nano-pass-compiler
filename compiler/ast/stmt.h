#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cctype>      // for isdigit, isalpha, isalnum
#include <stdexcept>   // for runtime_error
#include <memory>
#include "expr.h"
#include "../common/source_location.h"

using namespace std;

/* ===================== STATEMENTS ===================== */


struct Stmt {
    SourceLocation loc;
    virtual ~Stmt() = default;
    virtual void print(int d) = 0;
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
    unique_ptr<Expr> condition;
    unique_ptr<Stmt> thenBranch, elseBranch;
    IfStmt(unique_ptr<Expr>c,unique_ptr<Stmt>t,unique_ptr<Stmt>e)
        :condition(move(c)),thenBranch(move(t)),elseBranch(move(e)){}
    void print(int d){
        cout<<string(d,' ')<<"If\n";
        condition->print(d+2);
        thenBranch->print(d+2);
        if(elseBranch){
            cout<<string(d,' ')<<"Else\n";
            elseBranch->print(d+2);
        }
    }
};

//added on day 12
struct WhileStmt:Stmt{
    unique_ptr<Expr> condition;
    unique_ptr<Stmt> body;

    WhileStmt(unique_ptr<Expr> c, unique_ptr<Stmt> b)
        :condition(move(c)), body(move(b)) {}

    void print(int d){
        cout << string(d,' ') << "While\n";
        condition->print(d+2);
        body->print(d+2);
    }
};

//added on day 13
struct FunctionStmt:Stmt{
    string name;
    vector<string> params;
    unique_ptr<BlockStmt> body;

    FunctionStmt(string n,vector<string> p,unique_ptr<BlockStmt> b)
        :name(move(n)),params(move(p)),body(move(b)){}
    
    void print(int d){
        cout<<string(d,' ')<<"Function "<<name<<"\n";
        cout<<string(d+2,' ')<<"Params:";
        for(auto& p:params) cout<<" "<<p;
        cout<<"\n";
        body->print(d+2);
    }
};

struct ReturnStmt:Stmt{
    unique_ptr<Expr> value;
    ReturnStmt(unique_ptr<Expr> v):value(move(v)){}
    void print(int d){
        cout<<string(d,' ')<<"Return\n";
        if(value) value->print(d+2);
    }
};

//added on day 15
struct ForStmt : Stmt {
    unique_ptr<Stmt> init;
    unique_ptr<Expr> condition;
    unique_ptr<Expr> increment;
    unique_ptr<Stmt> body;

    ForStmt(unique_ptr<Stmt> i,
            unique_ptr<Expr> c,
            unique_ptr<Expr> inc,
            unique_ptr<Stmt> b)
        : init(move(i)), condition(move(c)), increment(move(inc)), body(move(b)) {}

    void print(int d){
        cout<<string(d,' ')<<"For\n";
        if(init) init->print(d+2);
        if(condition) condition->print(d+2);
        if(increment){
            cout<<string(d+2,' ')<<"Increment\n";
            increment->print(d+4);
        }
        body->print(d+2);
    }
};
