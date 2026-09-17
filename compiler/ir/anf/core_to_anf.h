#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ir/anf/anf_ir.h"
#include "ir/core/core_ir.h"

namespace nanopass {

class CoreToANFLowering {
  int tempCounter = 0;
  int shortCircuitCounter = 0;

public:
  ANFProgram lower(const CoreProgram &coreProg) {
    ANFProgram anfProg;
    tempCounter = 0;
    shortCircuitCounter = 0;

    for (const auto &stmt : coreProg.topLevelStmts) {
      lowerStmt(stmt.get(), anfProg.topLevelStmts);
    }

    for (const auto &fn : coreProg.functions) {
      anfProg.functions.push_back(lowerFunction(fn));
    }

    return anfProg;
  }

private:
  ANFFunction lowerFunction(const CoreFunction &fn) {
    tempCounter = 0; // Reset temporary IDs per function
    shortCircuitCounter = 0;

    ANFFunction anfFn;
    anfFn.name = fn.name;
    anfFn.returnType = fn.returnType;
    anfFn.params = fn.params;
    anfFn.origin = fn.origin;

    for (const auto &stmt : fn.body) {
      lowerStmt(stmt.get(), anfFn.body);
    }

    return anfFn;
  }

  void lowerStmt(CoreStmt *stmt, std::vector<std::unique_ptr<ANFStmt>> &out) {
    if (!stmt)
      return;

    if (auto *v = dynamic_cast<CoreVarDeclStmt *>(stmt)) {
      std::optional<ANFAtom> initAtom = std::nullopt;
      if (v->initializer) {
        initAtom = atomize(v->initializer.get(), out);
      }
      out.push_back(std::make_unique<ANFVarDeclStmt>(v->id, v->type, initAtom,
                                                     v->origin));
      return;
    }

    if (auto *a = dynamic_cast<CoreAssignStmt *>(stmt)) {
      ANFAtom val = atomize(a->value.get(), out);
      out.push_back(std::make_unique<ANFAssignStmt>(a->target, std::move(val),
                                                    a->origin));
      return;
    }

    if (auto *ia = dynamic_cast<CoreIndexAssignStmt *>(stmt)) {
      ANFAtom idx = atomize(ia->index.get(), out);
      ANFAtom val = atomize(ia->value.get(), out);
      out.push_back(std::make_unique<ANFIndexAssignStmt>(
          ia->targetArray, std::move(idx), std::move(val), ia->origin));
      return;
    }

    if (auto *e = dynamic_cast<CoreExprStmt *>(stmt)) {
      atomize(e->expr.get(), out);
      return;
    }

    if (auto *i = dynamic_cast<CoreIfStmt *>(stmt)) {
      ANFAtom cond = atomize(i->condition.get(), out);
      std::vector<std::unique_ptr<ANFStmt>> thenB;
      for (const auto &s : i->thenBranch)
        lowerStmt(s.get(), thenB);
      std::vector<std::unique_ptr<ANFStmt>> elseB;
      for (const auto &s : i->elseBranch)
        lowerStmt(s.get(), elseB);
      out.push_back(std::make_unique<ANFIfStmt>(
          std::move(cond), std::move(thenB), std::move(elseB), i->origin));
      return;
    }

    if (auto *w = dynamic_cast<CoreWhileStmt *>(stmt)) {
      std::vector<std::unique_ptr<ANFStmt>> condHeader;
      ANFAtom cond = atomize(w->condition.get(), condHeader);
      std::vector<std::unique_ptr<ANFStmt>> body;
      for (const auto &s : w->body)
        lowerStmt(s.get(), body);
      out.push_back(std::make_unique<ANFWhileStmt>(
          std::move(condHeader), std::move(cond), std::move(body), w->origin));
      return;
    }

    if (auto *r = dynamic_cast<CoreReturnStmt *>(stmt)) {
      std::optional<ANFAtom> val = std::nullopt;
      if (r->value) {
        val = atomize(r->value.get(), out);
      }
      out.push_back(std::make_unique<ANFReturnStmt>(std::move(val), r->origin));
      return;
    }

    if (auto *b = dynamic_cast<CoreBreakStmt *>(stmt)) {
      out.push_back(std::make_unique<ANFBreakStmt>(b->origin));
      return;
    }

    if (auto *c = dynamic_cast<CoreContinueStmt *>(stmt)) {
      out.push_back(std::make_unique<ANFContinueStmt>(c->origin));
      return;
    }
  }

  ANFAtom atomize(CoreExpr *expr, std::vector<std::unique_ptr<ANFStmt>> &out) {
    if (!expr)
      return ANFAtom::fromConstant(false, LangType::Unknown());

    if (auto *c = dynamic_cast<CoreConstantExpr *>(expr)) {
      return ANFAtom::fromConstant(c->value, c->type);
    }

    if (auto *v = dynamic_cast<CoreVariableRefExpr *>(expr)) {
      return ANFAtom::fromVariable(v->id, v->type);
    }

    if (auto *u = dynamic_cast<CoreUnaryExpr *>(expr)) {
      ANFAtom operandAtom = atomize(u->operand.get(), out);
      TemporaryId tmp{tempCounter++};
      out.push_back(std::make_unique<ANFComputeStmt>(ANFComputation::makeUnary(
          tmp, u->op, std::move(operandAtom), u->type, u->origin)));
      return ANFAtom::fromTemporary(tmp, u->type);
    }

    if (auto *b = dynamic_cast<CoreBinaryExpr *>(expr)) {
      // Short-circuit logical AND (&&)
      if (b->op == BinaryOpKind::LogicalAnd) {
        LocalId scVar{"_sc", shortCircuitCounter++};
        out.push_back(std::make_unique<ANFVarDeclStmt>(
            scVar, LangType::Bool(),
            ANFAtom::fromConstant(false, LangType::Bool()), b->origin));

        ANFAtom leftAtom = atomize(b->left.get(), out);

        std::vector<std::unique_ptr<ANFStmt>> thenBranch;
        ANFAtom rightAtom = atomize(b->right.get(), thenBranch);
        thenBranch.push_back(std::make_unique<ANFAssignStmt>(
            scVar, std::move(rightAtom), b->origin));

        out.push_back(std::make_unique<ANFIfStmt>(
            std::move(leftAtom), std::move(thenBranch),
            std::vector<std::unique_ptr<ANFStmt>>{}, b->origin));
        return ANFAtom::fromVariable(scVar, LangType::Bool());
      }

      // Short-circuit logical OR (||)
      if (b->op == BinaryOpKind::LogicalOr) {
        LocalId scVar{"_sc", shortCircuitCounter++};
        out.push_back(std::make_unique<ANFVarDeclStmt>(
            scVar, LangType::Bool(),
            ANFAtom::fromConstant(true, LangType::Bool()), b->origin));

        ANFAtom leftAtom = atomize(b->left.get(), out);

        TemporaryId notLeft{tempCounter++};
        out.push_back(std::make_unique<ANFComputeStmt>(ANFComputation::makeUnary(
            notLeft, UnaryOpKind::LogicalNot, leftAtom, LangType::Bool(),
            b->origin)));

        std::vector<std::unique_ptr<ANFStmt>> thenBranch;
        ANFAtom rightAtom = atomize(b->right.get(), thenBranch);
        thenBranch.push_back(std::make_unique<ANFAssignStmt>(
            scVar, std::move(rightAtom), b->origin));

        out.push_back(std::make_unique<ANFIfStmt>(
            ANFAtom::fromTemporary(notLeft, LangType::Bool()),
            std::move(thenBranch), std::vector<std::unique_ptr<ANFStmt>>{},
            b->origin));
        return ANFAtom::fromVariable(scVar, LangType::Bool());
      }

      // Ordinary binary arithmetic/comparison: strict left-to-right evaluation
      ANFAtom leftAtom = atomize(b->left.get(), out);
      ANFAtom rightAtom = atomize(b->right.get(), out);
      TemporaryId tmp{tempCounter++};
      out.push_back(std::make_unique<ANFComputeStmt>(ANFComputation::makeBinary(
          tmp, b->op, std::move(leftAtom), std::move(rightAtom), b->type,
          b->origin)));
      return ANFAtom::fromTemporary(tmp, b->type);
    }

    if (auto *call = dynamic_cast<CoreCallExpr *>(expr)) {
      std::vector<ANFAtom> args;
      for (const auto &arg : call->args) {
        args.push_back(atomize(arg.get(), out));
      }

      if (call->type.isFunction() || call->type.kind == LangTypeKind::Void) {
        out.push_back(std::make_unique<ANFComputeStmt>(ANFComputation::makeCall(
            std::nullopt, call->callee, std::move(args), call->type,
            call->origin)));
        return ANFAtom::fromConstant(false, LangType::Void());
      }

      TemporaryId tmp{tempCounter++};
      out.push_back(std::make_unique<ANFComputeStmt>(ANFComputation::makeCall(
          tmp, call->callee, std::move(args), call->type, call->origin)));
      return ANFAtom::fromTemporary(tmp, call->type);
    }

    if (auto *idx = dynamic_cast<CoreIndexExpr *>(expr)) {
      ANFAtom arrAtom = atomize(idx->array.get(), out);
      ANFAtom idxAtom = atomize(idx->index.get(), out);
      TemporaryId tmp{tempCounter++};
      out.push_back(std::make_unique<ANFComputeStmt>(ANFComputation::makeIndexLoad(
          tmp, arrAtom.variableId, std::move(idxAtom), idx->type, idx->origin)));
      return ANFAtom::fromTemporary(tmp, idx->type);
    }

    return ANFAtom::fromConstant(false, LangType::Unknown());
  }
};

} // namespace nanopass
