#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/core/core_ir.h"

namespace nanopass {

struct CoreVerifierResult {
  bool ok = true;
  std::vector<std::string> errors;

  void addError(std::string err) {
    ok = false;
    errors.push_back(std::move(err));
  }
};

class CoreVerifier {
  CoreVerifierResult result;
  std::unordered_map<std::string, LangType> knownFunctions;
  std::vector<std::unordered_map<std::string, LangType>> scopes;
  int loopDepth = 0;
  LangType currentReturnType = LangType::Unknown();

public:
  CoreVerifierResult verify(const CoreProgram &program) {
    result = CoreVerifierResult{};
    knownFunctions.clear();
    scopes.clear();
    loopDepth = 0;

    // Builtin print function
    knownFunctions["print"] = LangType::Void();

    // Check function uniqueness
    for (const auto &fn : program.functions) {
      if (knownFunctions.count(fn.name)) {
        result.addError("Duplicate function definition: @" + fn.name);
      } else {
        knownFunctions[fn.name] = fn.returnType;
      }
    }

    // Verify top-level statements
    enterScope();
    for (const auto &stmt : program.topLevelStmts) {
      verifyStmt(stmt.get());
    }
    exitScope();

    // Verify functions
    for (const auto &fn : program.functions) {
      verifyFunction(fn);
    }

    return result;
  }

private:
  void enterScope() { scopes.emplace_back(); }
  void exitScope() {
    if (!scopes.empty())
      scopes.pop_back();
  }

  void declare(const LocalId &id, const LangType &type) {
    scopes.back()[id.str()] = type;
  }

  bool isDeclared(const LocalId &id, LangType &outType) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto f = it->find(id.str());
      if (f != it->end()) {
        outType = f->second;
        return true;
      }
    }
    return false;
  }

  void verifyFunction(const CoreFunction &fn) {
    currentReturnType = fn.returnType;
    loopDepth = 0;
    enterScope();

    std::unordered_set<std::string> paramNames;
    for (const auto &param : fn.params) {
      if (!paramNames.insert(param.id.str()).second) {
        result.addError("Duplicate parameter ID in function @" + fn.name + ": " +
                        param.id.str());
      }
      declare(param.id, param.type);
    }

    for (const auto &stmt : fn.body) {
      verifyStmt(stmt.get());
    }

    exitScope();
  }

  void verifyStmt(CoreStmt *stmt) {
    if (!stmt)
      return;

    if (auto *v = dynamic_cast<CoreVarDeclStmt *>(stmt)) {
      LangType existing;
      if (isDeclared(v->id, existing)) {
        result.addError("Duplicate variable declaration: " + v->id.str());
      }
      declare(v->id, v->type);
      if (v->initializer) {
        verifyExpr(v->initializer.get());
      }
      return;
    }

    if (auto *a = dynamic_cast<CoreAssignStmt *>(stmt)) {
      LangType targetType;
      if (!isDeclared(a->target, targetType)) {
        result.addError("Assignment to undeclared variable: " + a->target.str());
      }
      if (a->value) {
        verifyExpr(a->value.get());
      }
      return;
    }

    if (auto *ia = dynamic_cast<CoreIndexAssignStmt *>(stmt)) {
      LangType targetType;
      if (!isDeclared(ia->targetArray, targetType)) {
        result.addError("Assignment to undeclared array: " +
                        ia->targetArray.str());
      }
      if (ia->index)
        verifyExpr(ia->index.get());
      if (ia->value)
        verifyExpr(ia->value.get());
      return;
    }

    if (auto *e = dynamic_cast<CoreExprStmt *>(stmt)) {
      if (e->expr)
        verifyExpr(e->expr.get());
      return;
    }

    if (auto *i = dynamic_cast<CoreIfStmt *>(stmt)) {
      if (i->condition)
        verifyExpr(i->condition.get());
      enterScope();
      for (const auto &s : i->thenBranch)
        verifyStmt(s.get());
      exitScope();

      enterScope();
      for (const auto &s : i->elseBranch)
        verifyStmt(s.get());
      exitScope();
      return;
    }

    if (auto *w = dynamic_cast<CoreWhileStmt *>(stmt)) {
      if (w->condition)
        verifyExpr(w->condition.get());
      loopDepth++;
      enterScope();
      for (const auto &s : w->body)
        verifyStmt(s.get());
      exitScope();
      loopDepth--;
      return;
    }

    if (auto *r = dynamic_cast<CoreReturnStmt *>(stmt)) {
      if (r->value) {
        verifyExpr(r->value.get());
      }
      return;
    }

    if (dynamic_cast<CoreBreakStmt *>(stmt)) {
      if (loopDepth <= 0) {
        result.addError("Break statement outside of loop");
      }
      return;
    }

    if (dynamic_cast<CoreContinueStmt *>(stmt)) {
      if (loopDepth <= 0) {
        result.addError("Continue statement outside of loop");
      }
      return;
    }

    result.addError("Unknown CoreStmt kind in CoreVerifier");
  }

  void verifyExpr(CoreExpr *expr) {
    if (!expr)
      return;

    if (dynamic_cast<CoreConstantExpr *>(expr))
      return;

    if (auto *v = dynamic_cast<CoreVariableRefExpr *>(expr)) {
      LangType t;
      if (!isDeclared(v->id, t)) {
        result.addError("Reference to undeclared variable: " + v->id.str());
      }
      return;
    }

    if (auto *idx = dynamic_cast<CoreIndexExpr *>(expr)) {
      if (idx->array)
        verifyExpr(idx->array.get());
      if (idx->index)
        verifyExpr(idx->index.get());
      return;
    }

    if (auto *u = dynamic_cast<CoreUnaryExpr *>(expr)) {
      if (u->operand)
        verifyExpr(u->operand.get());
      return;
    }

    if (auto *b = dynamic_cast<CoreBinaryExpr *>(expr)) {
      if (b->left)
        verifyExpr(b->left.get());
      if (b->right)
        verifyExpr(b->right.get());
      return;
    }

    if (auto *c = dynamic_cast<CoreCallExpr *>(expr)) {
      if (!knownFunctions.count(c->callee)) {
        result.addError("Call to unknown function: @" + c->callee);
      }
      for (const auto &arg : c->args) {
        if (arg)
          verifyExpr(arg.get());
      }
      return;
    }

    result.addError("Unknown CoreExpr kind in CoreVerifier");
  }
};

inline CoreVerifierResult verifyCoreIR(const CoreProgram &program) {
  CoreVerifier verifier;
  return verifier.verify(program);
}

} // namespace nanopass
