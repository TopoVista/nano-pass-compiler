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

//======PASS 1 : FOR -> WHILE DESUGARING========//  
struct DesugarForPass {

    unique_ptr<Stmt> transform(unique_ptr<Stmt> stmt){
        if(auto f = dynamic_cast<ForStmt*>(stmt.get())){
            return desugarFor(f);
        }

        if(auto b = dynamic_cast<BlockStmt*>(stmt.get())){
            auto nb = make_unique<BlockStmt>();
            for(auto& s : b->stmts)
                nb->stmts.push_back(transform(move(s)));
            return nb;
        }

        if(auto w = dynamic_cast<WhileStmt*>(stmt.get())){
            return make_unique<WhileStmt>(
                move(w->condition),
                transform(move(w->body))
            );
        }

        if(auto i = dynamic_cast<IfStmt*>(stmt.get())){
            return make_unique<IfStmt>(
                move(i->condition),
                transform(move(i->thenBranch)),
                i->elseBranch ? transform(move(i->elseBranch)) : nullptr
            );
        }

        // all other statements stay unchanged
        return stmt;
    }

private:
    unique_ptr<Stmt> desugarFor(ForStmt* f){
        /*
        for (init; cond; inc) body

        =>
        {
            init;
            while (cond) {
                body;
                inc;
            }
        }
        */

        auto block = make_unique<BlockStmt>();

        if(f->init)
            block->stmts.push_back(transform(move(f->init)));

        unique_ptr<Stmt> newBody;

        if(f->increment){
            auto bodyBlock = make_unique<BlockStmt>();
            bodyBlock->stmts.push_back(transform(move(f->body)));
            bodyBlock->stmts.push_back(
                make_unique<ExprStmt>(move(f->increment))
            );
            newBody = move(bodyBlock);
        } else {
            newBody = transform(move(f->body));
        }

        auto whileStmt = make_unique<WhileStmt>(
            f->condition ? move(f->condition)
                         : make_unique<NumberExpr>(1),
            move(newBody)
        );

        block->stmts.push_back(move(whileStmt));
        return block;
    }
};

