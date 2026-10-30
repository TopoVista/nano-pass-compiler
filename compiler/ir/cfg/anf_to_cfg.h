#pragma once

#include <memory>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ir/anf/anf_ir.h"
#include "ir/cfg/cfg_builder.h"
#include "ir/cfg/cfg_ir.h"

namespace nanopass {

class ANFToCFGLowering {
  std::unordered_map<int, ValueId> tempToVal;
  std::map<LocalId, LangType> memoryLocals_;

public:
  CFGProgram lower(const ANFProgram &anfProg) {
    CFGProgram cfgProg;

    // Lower functions
    for (const auto &fn : anfProg.functions) {
      cfgProg.functions.push_back(lowerFunction(fn));
    }

    // If there are top-level statements, wrap them in a synthetic @main if needed
    if (!anfProg.topLevelStmts.empty()) {
      ANFFunction topFn;
      topFn.name = "main";
      topFn.returnType = LangType::Void();
      // Top-level statements are not yet lowered to CFG (all user code is in functions).
      (void)topFn; // suppress unused warning; reserved for future use
      // Usually all user code in Nano compiles into functions; topLevelStmts handled if present
    }

    return cfgProg;
  }

private:
  CFGFunction lowerFunction(const ANFFunction &anfFn) {
    CFGFunction cfgFn;
    cfgFn.name = anfFn.name;
    cfgFn.returnType = anfFn.returnType;
    cfgFn.params = anfFn.params;
    cfgFn.origin = anfFn.origin;

    tempToVal.clear();
    memoryLocals_.clear();
    CFGBuilder builder(cfgFn);

    BlockId entryBB = builder.createBlock("entry");
    cfgFn.entryBlock = entryBB;
    builder.setInsertPoint(entryBB);

    for (const auto &stmt : anfFn.body) {
      lowerStmt(stmt.get(), builder);
    }

    if (!builder.currentBlockTerminated()) {
      if (anfFn.returnType.kind == LangTypeKind::Void) {
        builder.terminateWithReturn(std::nullopt);
      } else {
        builder.terminateWithReturn(
            CFGOperand::fromConstant(int64_t(0), anfFn.returnType));
      }
    }

    builder.finalizeEdges();
    cfgFn.memoryLocals = std::move(memoryLocals_);
    return cfgFn;
  }

  void lowerStmt(ANFStmt *stmt, CFGBuilder &builder) {
    if (!stmt || builder.currentBlockTerminated())
      return;

    if (auto *c = dynamic_cast<ANFComputeStmt *>(stmt)) {
      lowerComputation(c->comp, builder);
      return;
    }

    if (auto *v = dynamic_cast<ANFVarDeclStmt *>(stmt)) {
      if (v->type.kind == LangTypeKind::Array)
        memoryLocals_.emplace(v->id, v->type);
      if (v->init) {
        CFGOperand val = lowerAtom(*v->init, builder);
        builder.emitInstruction(
            CFGInstruction::makeStoreLocal(v->id, std::move(val), v->origin));
      }
      return;
    }

    if (auto *a = dynamic_cast<ANFAssignStmt *>(stmt)) {
      CFGOperand val = lowerAtom(a->value, builder);
      builder.emitInstruction(
          CFGInstruction::makeStoreLocal(a->target, std::move(val), a->origin));
      return;
    }

    if (auto *ia = dynamic_cast<ANFIndexAssignStmt *>(stmt)) {
      CFGOperand idx = lowerAtom(ia->index, builder);
      CFGOperand val = lowerAtom(ia->value, builder);
      builder.emitInstruction(CFGInstruction::makeStoreIndex(
          ia->targetArray, std::move(idx), std::move(val), ia->origin));
      return;
    }

    if (auto *i = dynamic_cast<ANFIfStmt *>(stmt)) {
      CFGOperand cond = lowerAtom(i->condition, builder);
      BlockId thenBB = builder.createBlock("then");
      BlockId elseBB =
          i->elseBranch.empty() ? BlockId{-1} : builder.createBlock("else");
      BlockId mergeBB = builder.createBlock("if.merge");

      builder.terminateWithBranch(std::move(cond), thenBB,
                                  i->elseBranch.empty() ? mergeBB : elseBB);

      // Then block
      builder.setInsertPoint(thenBB);
      for (const auto &s : i->thenBranch)
        lowerStmt(s.get(), builder);
      if (!builder.currentBlockTerminated())
        builder.terminateWithJump(mergeBB);

      // Else block
      if (!i->elseBranch.empty()) {
        builder.setInsertPoint(elseBB);
        for (const auto &s : i->elseBranch)
          lowerStmt(s.get(), builder);
        if (!builder.currentBlockTerminated())
          builder.terminateWithJump(mergeBB);
      }

      builder.setInsertPoint(mergeBB);
      return;
    }

    if (auto *w = dynamic_cast<ANFWhileStmt *>(stmt)) {
      BlockId headerBB = builder.createBlock("loop.header");
      BlockId bodyBB = builder.createBlock("loop.body");
      BlockId exitBB = builder.createBlock("loop.exit");

      builder.terminateWithJump(headerBB);

      // Header block
      builder.setInsertPoint(headerBB);
      for (const auto &s : w->condHeader)
        lowerStmt(s.get(), builder);

      CFGOperand cond = lowerAtom(w->condition, builder);
      builder.terminateWithBranch(std::move(cond), bodyBB, exitBB);

      // Body block
      builder.pushLoop(headerBB, exitBB);
      builder.setInsertPoint(bodyBB);
      for (const auto &s : w->body)
        lowerStmt(s.get(), builder);
      if (!builder.currentBlockTerminated())
        builder.terminateWithJump(headerBB);
      builder.popLoop();

      // Exit block
      builder.setInsertPoint(exitBB);
      return;
    }

    if (auto *r = dynamic_cast<ANFReturnStmt *>(stmt)) {
      std::optional<CFGOperand> val = std::nullopt;
      if (r->value)
        val = lowerAtom(*r->value, builder);
      builder.terminateWithReturn(std::move(val));
      return;
    }

    if (dynamic_cast<ANFBreakStmt *>(stmt)) {
      const auto *loop = builder.currentLoop();
      if (loop)
        builder.terminateWithJump(loop->breakTarget);
      return;
    }

    if (dynamic_cast<ANFContinueStmt *>(stmt)) {
      const auto *loop = builder.currentLoop();
      if (loop)
        builder.terminateWithJump(loop->continueTarget);
      return;
    }
  }

  void lowerComputation(const ANFComputation &comp, CFGBuilder &builder) {
    switch (comp.kind) {
    case ANFComputationKind::Binary: {
      ValueId dst = builder.allocateValueId();
      if (comp.dst)
        tempToVal[comp.dst->index] = dst;
      CFGOperand l = lowerAtom(comp.left, builder);
      CFGOperand r = lowerAtom(comp.right, builder);
      builder.emitInstruction(CFGInstruction::makeBinary(
          dst, comp.binOp, std::move(l), std::move(r), comp.type, comp.origin));
      break;
    }
    case ANFComputationKind::Unary: {
      ValueId dst = builder.allocateValueId();
      if (comp.dst)
        tempToVal[comp.dst->index] = dst;
      CFGOperand op = lowerAtom(comp.operand, builder);
      builder.emitInstruction(CFGInstruction::makeUnary(
          dst, comp.unOp, std::move(op), comp.type, comp.origin));
      break;
    }
    case ANFComputationKind::Call: {
      std::vector<CFGOperand> args;
      for (const auto &arg : comp.args)
        args.push_back(lowerAtom(arg, builder));

      std::optional<ValueId> dst = std::nullopt;
      if (comp.dst) {
        ValueId v = builder.allocateValueId();
        tempToVal[comp.dst->index] = v;
        dst = v;
      }
      builder.emitInstruction(CFGInstruction::makeCall(
          dst, comp.callee, std::move(args), comp.type, comp.origin));
      break;
    }
    case ANFComputationKind::IndexLoad: {
      ValueId dst = builder.allocateValueId();
      if (comp.dst)
        tempToVal[comp.dst->index] = dst;
      CFGOperand idx = lowerAtom(comp.index, builder);
      builder.emitInstruction(CFGInstruction::makeLoadIndex(
          dst, comp.array, std::move(idx), comp.type, comp.origin));
      break;
    }
    }
  }

  CFGOperand lowerAtom(const ANFAtom &atom, CFGBuilder &builder) {
    switch (atom.kind) {
    case ANFAtomKind::Constant:
      return CFGOperand::fromConstant(atom.constantValue, atom.type);
    case ANFAtomKind::Temporary: {
      auto it = tempToVal.find(atom.temporaryId.index);
      if (it != tempToVal.end()) {
        return CFGOperand::fromValue(it->second, atom.type);
      }
      ValueId v{atom.temporaryId.index};
      return CFGOperand::fromValue(v, atom.type);
    }
    case ANFAtomKind::Variable: {
      ValueId v = builder.allocateValueId();
      builder.emitInstruction(
          CFGInstruction::makeLoadLocal(v, atom.variableId, atom.type));
      return CFGOperand::fromValue(v, atom.type);
    }
    }
    return CFGOperand::fromConstant(false, LangType::Unknown());
  }
};

} // namespace nanopass
