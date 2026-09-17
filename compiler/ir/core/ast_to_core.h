#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast/expr.h"
#include "ast/stmt.h"
#include "common/error.h"
#include "ir/core/core_ir.h"

namespace nanopass {

class ASTToCoreLowering {
  std::vector<std::unordered_map<std::string, LocalId>> scopes;
  std::unordered_map<std::string, int> nameCounters;

public:
  CoreProgram lower(const std::vector<std::unique_ptr<Stmt>> &program) {
    CoreProgram coreProg;
    enterScope(); // global scope

    // First pass: register function names so calls know their targets
    for (const auto &stmt : program) {
      if (auto *fn = dynamic_cast<FunctionStmt *>(stmt.get())) {
        LocalId id{fn->name, 0};
        scopes.front()[fn->name] = id;
      }
    }

    // Second pass: lower functions and top-level statements
    for (const auto &stmt : program) {
      if (auto *fn = dynamic_cast<FunctionStmt *>(stmt.get())) {
        coreProg.functions.push_back(lowerFunction(fn));
      } else {
        auto lowered = lowerStmt(stmt.get());
        for (auto &s : lowered) {
          coreProg.topLevelStmts.push_back(std::move(s));
        }
      }
    }

    exitScope();
    return coreProg;
  }

private:
  void enterScope() { scopes.emplace_back(); }

  void exitScope() {
    if (scopes.empty())
      throw std::runtime_error("ASTToCoreLowering: underflow in scope stack");
    scopes.pop_back();
  }

  LocalId declareVar(const std::string &name) {
    int idx = nameCounters[name]++;
    LocalId id{name, idx};
    scopes.back()[name] = id;
    return id;
  }

  LocalId lookup(const std::string &name, SourceLocation loc = {}) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto found = it->find(name);
      if (found != it->end())
        return found->second;
    }
    // Fallback: if not found in local scopes (e.g. global / implicit)
    return LocalId{name, 0};
  }

  CoreFunction lowerFunction(FunctionStmt *fn) {
    CoreFunction coreFn;
    coreFn.name = fn->name;
    coreFn.returnType = fn->returnType;
    coreFn.origin = SourceOrigin(fn->loc, "ast_to_core");

    enterScope();

    for (const auto &param : fn->params) {
      LocalId id = declareVar(param.first);
      coreFn.params.push_back(CoreParam{id, param.second});
    }

    if (fn->body) {
      for (const auto &s : fn->body->stmts) {
        auto lowered = lowerStmt(s.get());
        for (auto &st : lowered) {
          coreFn.body.push_back(std::move(st));
        }
      }
    }

    exitScope();
    return coreFn;
  }

  std::vector<std::unique_ptr<CoreStmt>> lowerStmt(Stmt *stmt) {
    std::vector<std::unique_ptr<CoreStmt>> out;
    if (!stmt)
      return out;

    SourceOrigin origin(stmt->loc, "ast_to_core");

    if (auto *s = dynamic_cast<VarDeclStmt *>(stmt)) {
      LocalId id = declareVar(s->name);
      std::unique_ptr<CoreExpr> initExpr = nullptr;
      if (s->initializer)
        initExpr = lowerExpr(s->initializer.get());
      out.push_back(std::make_unique<CoreVarDeclStmt>(id, s->type,
                                                      std::move(initExpr), origin));
      return out;
    }

    if (auto *s = dynamic_cast<BlockStmt *>(stmt)) {
      enterScope();
      for (const auto &child : s->stmts) {
        auto lowered = lowerStmt(child.get());
        for (auto &c : lowered)
          out.push_back(std::move(c));
      }
      exitScope();
      return out;
    }

    if (auto *s = dynamic_cast<ExprStmt *>(stmt)) {
      // Check for assignment: x = rhs OR arr[idx] = rhs
      if (auto *b = dynamic_cast<BinaryExpr *>(s->e.get())) {
        if (b->op == "=") {
          if (auto *v = dynamic_cast<VariableExpr *>(b->left.get())) {
            LocalId tgt = lookup(v->name, v->loc);
            auto val = lowerExpr(b->right.get());
            out.push_back(std::make_unique<CoreAssignStmt>(
                tgt, std::move(val), origin));
            return out;
          }
          if (auto *idx = dynamic_cast<IndexExpr *>(b->left.get())) {
            if (auto *arrVar = dynamic_cast<VariableExpr *>(idx->array.get())) {
              LocalId tgtArr = lookup(arrVar->name, arrVar->loc);
              auto idxExpr = lowerExpr(idx->index.get());
              auto val = lowerExpr(b->right.get());
              out.push_back(std::make_unique<CoreIndexAssignStmt>(
                  tgtArr, std::move(idxExpr), std::move(val), origin));
              return out;
            }
          }
        }
      }
      out.push_back(
          std::make_unique<CoreExprStmt>(lowerExpr(s->e.get()), origin));
      return out;
    }

    if (auto *s = dynamic_cast<PrintStmt *>(stmt)) {
      std::vector<std::unique_ptr<CoreExpr>> args;
      args.push_back(lowerExpr(s->e.get()));
      auto call = std::make_unique<CoreCallExpr>("print", std::move(args),
                                                LangType::Void(), origin);
      out.push_back(std::make_unique<CoreExprStmt>(std::move(call), origin));
      return out;
    }

    if (auto *s = dynamic_cast<IfStmt *>(stmt)) {
      auto cond = lowerExpr(s->condition.get());
      std::vector<std::unique_ptr<CoreStmt>> thenB;
      if (s->thenBranch) {
        thenB = lowerStmt(s->thenBranch.get());
      }
      std::vector<std::unique_ptr<CoreStmt>> elseB;
      if (s->elseBranch) {
        elseB = lowerStmt(s->elseBranch.get());
      }
      out.push_back(std::make_unique<CoreIfStmt>(
          std::move(cond), std::move(thenB), std::move(elseB), origin));
      return out;
    }

    if (auto *s = dynamic_cast<WhileStmt *>(stmt)) {
      auto cond = lowerExpr(s->condition.get());
      std::vector<std::unique_ptr<CoreStmt>> body;
      if (s->body) {
        body = lowerStmt(s->body.get());
      }
      out.push_back(std::make_unique<CoreWhileStmt>(
          std::move(cond), std::move(body), origin));
      return out;
    }

    if (auto *s = dynamic_cast<ReturnStmt *>(stmt)) {
      std::unique_ptr<CoreExpr> retVal = nullptr;
      if (s->value)
        retVal = lowerExpr(s->value.get());
      out.push_back(std::make_unique<CoreReturnStmt>(std::move(retVal), origin));
      return out;
    }

    if (dynamic_cast<BreakStmt *>(stmt)) {
      out.push_back(std::make_unique<CoreBreakStmt>(origin));
      return out;
    }

    if (dynamic_cast<ContinueStmt *>(stmt)) {
      out.push_back(std::make_unique<CoreContinueStmt>(origin));
      return out;
    }

    if (dynamic_cast<ForStmt *>(stmt)) {
      throw CompileError("Internal error: ForStmt encountered in ASTToCoreLowering; "
                         "all ForStmt must be desugared before CoreIR lowering",
                         stmt->loc.line, stmt->loc.col);
    }

    throw CompileError("Unhandled AST statement in ASTToCoreLowering",
                       stmt->loc.line, stmt->loc.col);
  }

  std::unique_ptr<CoreExpr> lowerExpr(Expr *expr) {
    if (!expr)
      return nullptr;

    SourceOrigin origin(expr->loc, "ast_to_core");

    if (auto *n = dynamic_cast<NumberExpr *>(expr)) {
      if (n->isFloat) {
        return std::make_unique<CoreConstantExpr>(
            n->floatValue, LangType::Float(64), origin);
      }
      return std::make_unique<CoreConstantExpr>(
          static_cast<int64_t>(n->intValue), LangType::Int(32), origin);
    }

    if (auto *b = dynamic_cast<BoolExpr *>(expr)) {
      return std::make_unique<CoreConstantExpr>(b->value, LangType::Bool(),
                                                origin);
    }

    if (auto *s = dynamic_cast<StringExpr *>(expr)) {
      return std::make_unique<CoreConstantExpr>(s->value, LangType::String(),
                                                origin);
    }

    if (auto *v = dynamic_cast<VariableExpr *>(expr)) {
      LocalId id = lookup(v->name, v->loc);
      return std::make_unique<CoreVariableRefExpr>(id, v->type, origin);
    }

    if (auto *idx = dynamic_cast<IndexExpr *>(expr)) {
      auto arr = lowerExpr(idx->array.get());
      auto index = lowerExpr(idx->index.get());
      return std::make_unique<CoreIndexExpr>(std::move(arr), std::move(index),
                                             expr->type, origin);
    }

    if (auto *u = dynamic_cast<UnaryExpr *>(expr)) {
      UnaryOpKind op;
      if (u->op == "-") {
        op = UnaryOpKind::Negate;
      } else if (u->op == "!") {
        op = UnaryOpKind::LogicalNot;
      } else {
        throw CompileError("Unknown unary op: " + u->op, u->loc.line, u->loc.col);
      }
      auto operand = lowerExpr(u->right.get());
      return std::make_unique<CoreUnaryExpr>(op, std::move(operand), expr->type,
                                             origin);
    }

    if (auto *b = dynamic_cast<BinaryExpr *>(expr)) {
      BinaryOpKind op;
      if (b->op == "+") op = BinaryOpKind::Add;
      else if (b->op == "-") op = BinaryOpKind::Sub;
      else if (b->op == "*") op = BinaryOpKind::Mul;
      else if (b->op == "/") op = BinaryOpKind::Div;
      else if (b->op == "%") op = BinaryOpKind::Mod;
      else if (b->op == "==") op = BinaryOpKind::Eq;
      else if (b->op == "!=") op = BinaryOpKind::Ne;
      else if (b->op == "<") op = BinaryOpKind::Lt;
      else if (b->op == "<=") op = BinaryOpKind::Le;
      else if (b->op == ">") op = BinaryOpKind::Gt;
      else if (b->op == ">=") op = BinaryOpKind::Ge;
      else if (b->op == "&&") op = BinaryOpKind::LogicalAnd;
      else if (b->op == "||") op = BinaryOpKind::LogicalOr;
      else {
        throw CompileError("Unknown binary op in ASTToCoreLowering: " + b->op,
                           b->loc.line, b->loc.col);
      }

      auto left = lowerExpr(b->left.get());
      auto right = lowerExpr(b->right.get());
      return std::make_unique<CoreBinaryExpr>(op, std::move(left),
                                              std::move(right), expr->type,
                                              origin);
    }

    if (auto *c = dynamic_cast<CallExpr *>(expr)) {
      std::vector<std::unique_ptr<CoreExpr>> args;
      for (const auto &a : c->args) {
        args.push_back(lowerExpr(a.get()));
      }
      return std::make_unique<CoreCallExpr>(c->callee, std::move(args),
                                            expr->type, origin);
    }

    throw CompileError("Unhandled expression kind in ASTToCoreLowering",
                       expr->loc.line, expr->loc.col);
  }
};

} // namespace nanopass
