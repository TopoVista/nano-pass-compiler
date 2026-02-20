#pragma once

#include "../ast/expr.h"
#include "../ast/stmt.h"
#include "../lexer/lexer.h"
#include <cctype> // for isdigit, isalpha, isalnum
#include <iostream>
#include <memory>
#include <stdexcept> // for runtime_error
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

//======PASS 1 : FOR -> WHILE DESUGARING========//
struct DesugarForPass {

  unique_ptr<Stmt> transform(unique_ptr<Stmt> stmt) {
    if (auto f = dynamic_cast<ForStmt *>(stmt.get())) {
      return desugarFor(f);
    }

    if (auto b = dynamic_cast<BlockStmt *>(stmt.get())) {
      auto nb = make_unique<BlockStmt>();
      for (auto &s : b->stmts)
        nb->stmts.push_back(transform(std::move(s)));
      return nb;
    }

    if (auto w = dynamic_cast<WhileStmt *>(stmt.get())) {
      return make_unique<WhileStmt>(std::move(w->condition),
                                    transform(std::move(w->body)));
    }

    if (auto i = dynamic_cast<IfStmt *>(stmt.get())) {
      return make_unique<IfStmt>(
          std::move(i->condition), transform(std::move(i->thenBranch)),
          i->elseBranch ? transform(std::move(i->elseBranch)) : nullptr);
    }

    // all other statements stay unchanged
    return stmt;
  }

private:
  unique_ptr<Stmt> desugarFor(ForStmt *f) {
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

    if (f->init)
      block->stmts.push_back(transform(std::move(f->init)));

    unique_ptr<Stmt> newBody;

    if (f->increment) {
      auto bodyBlock = make_unique<BlockStmt>();
      bodyBlock->stmts.push_back(transform(std::move(f->body)));
      bodyBlock->stmts.push_back(
          make_unique<ExprStmt>(std::move(f->increment)));
      newBody = std::move(bodyBlock);
    } else {
      newBody = transform(std::move(f->body));
    }

    auto whileStmt = make_unique<WhileStmt>(
        f->condition ? std::move(f->condition) : make_unique<NumberExpr>(1),
        std::move(newBody));

    block->stmts.push_back(std::move(whileStmt));
    return block;
  }
};
