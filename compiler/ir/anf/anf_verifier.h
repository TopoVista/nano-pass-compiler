#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/anf/anf_ir.h"

namespace nanopass {

struct ANFVerifierResult {
  bool ok = true;
  std::vector<std::string> errors;

  void addError(std::string err) {
    ok = false;
    errors.push_back(std::move(err));
  }
};

class ANFVerifier {
  ANFVerifierResult result;
  std::unordered_set<std::string> definedTemporaries;
  std::vector<std::unordered_set<std::string>> localScopes;
  int loopDepth = 0;

public:
  ANFVerifierResult verify(const ANFProgram &program) {
    result = ANFVerifierResult{};

    enterScope();
    for (const auto &stmt : program.topLevelStmts) {
      verifyStmt(stmt.get());
    }
    exitScope();

    for (const auto &fn : program.functions) {
      verifyFunction(fn);
    }

    return result;
  }

private:
  void enterScope() { localScopes.emplace_back(); }
  void exitScope() {
    if (!localScopes.empty())
      localScopes.pop_back();
  }

  void declareLocal(const LocalId &id) {
    localScopes.back().insert(id.str());
  }

  bool isLocalDeclared(const LocalId &id) {
    for (auto it = localScopes.rbegin(); it != localScopes.rend(); ++it) {
      if (it->count(id.str()))
        return true;
    }
    return false;
  }

  void verifyFunction(const ANFFunction &fn) {
    definedTemporaries.clear();
    loopDepth = 0;
    enterScope();

    for (const auto &p : fn.params) {
      declareLocal(p.id);
    }

    for (const auto &stmt : fn.body) {
      verifyStmt(stmt.get());
    }

    exitScope();
  }

  void verifyStmt(ANFStmt *stmt) {
    if (!stmt)
      return;

    if (auto *c = dynamic_cast<ANFComputeStmt *>(stmt)) {
      verifyComputation(c->comp);
      return;
    }

    if (auto *v = dynamic_cast<ANFVarDeclStmt *>(stmt)) {
      declareLocal(v->id);
      if (v->init) {
        verifyAtom(*v->init);
      }
      return;
    }

    if (auto *a = dynamic_cast<ANFAssignStmt *>(stmt)) {
      if (!isLocalDeclared(a->target)) {
        result.addError("Assignment to undeclared local: " + a->target.str());
      }
      verifyAtom(a->value);
      return;
    }

    if (auto *ia = dynamic_cast<ANFIndexAssignStmt *>(stmt)) {
      if (!isLocalDeclared(ia->targetArray)) {
        result.addError("Assignment to undeclared array: " +
                        ia->targetArray.str());
      }
      verifyAtom(ia->index);
      verifyAtom(ia->value);
      return;
    }

    if (auto *i = dynamic_cast<ANFIfStmt *>(stmt)) {
      verifyAtom(i->condition);
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

    if (auto *w = dynamic_cast<ANFWhileStmt *>(stmt)) {
      loopDepth++;
      enterScope();
      for (const auto &s : w->condHeader)
        verifyStmt(s.get());
      verifyAtom(w->condition);

      for (const auto &s : w->body)
        verifyStmt(s.get());
      exitScope();
      loopDepth--;
      return;
    }

    if (auto *r = dynamic_cast<ANFReturnStmt *>(stmt)) {
      if (r->value) {
        verifyAtom(*r->value);
      }
      return;
    }

    if (dynamic_cast<ANFBreakStmt *>(stmt)) {
      if (loopDepth <= 0) {
        result.addError("Break outside of loop in ANF");
      }
      return;
    }

    if (dynamic_cast<ANFContinueStmt *>(stmt)) {
      if (loopDepth <= 0) {
        result.addError("Continue outside of loop in ANF");
      }
      return;
    }

    result.addError("Unknown ANFStmt kind in ANFVerifier");
  }

  void verifyComputation(const ANFComputation &comp) {
    if (comp.dst) {
      std::string dstStr = comp.dst->str();
      if (definedTemporaries.count(dstStr)) {
        result.addError("Temporary defined more than once: " + dstStr);
      }
      definedTemporaries.insert(dstStr);
    }

    switch (comp.kind) {
    case ANFComputationKind::Binary:
      verifyAtom(comp.left);
      verifyAtom(comp.right);
      break;
    case ANFComputationKind::Unary:
      verifyAtom(comp.operand);
      break;
    case ANFComputationKind::Call:
      for (const auto &arg : comp.args)
        verifyAtom(arg);
      break;
    case ANFComputationKind::IndexLoad:
      if (!isLocalDeclared(comp.array)) {
        result.addError("IndexLoad from undeclared array: " + comp.array.str());
      }
      verifyAtom(comp.index);
      break;
    }
  }

  void verifyAtom(const ANFAtom &atom) {
    if (atom.kind == ANFAtomKind::Temporary) {
      if (!definedTemporaries.count(atom.temporaryId.str())) {
        result.addError("Use of undefined temporary: " +
                        atom.temporaryId.str());
      }
    } else if (atom.kind == ANFAtomKind::Variable) {
      if (!isLocalDeclared(atom.variableId)) {
        result.addError("Use of undeclared variable in atom: " +
                        atom.variableId.str());
      }
    }
  }
};

inline ANFVerifierResult verifyANF(const ANFProgram &program) {
  ANFVerifier verifier;
  return verifier.verify(program);
}

} // namespace nanopass
