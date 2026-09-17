#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ir/cfg/cfg_ir.h"

namespace nanopass {

struct LoopContext {
  BlockId continueTarget;
  BlockId breakTarget;
};

class CFGBuilder {
  CFGFunction &fn;
  int nextBlockIndex = 0;
  int nextValueIndex = 0;
  BlockId currentBlockId;
  std::vector<LoopContext> loopStack;

public:
  explicit CFGBuilder(CFGFunction &function) : fn(function) {}

  BlockId createBlock(std::string label = "") {
    BlockId id{nextBlockIndex++};
    auto block = std::make_unique<BasicBlock>();
    block->id = id;
    block->label = std::move(label);

    fn.blockIndexMap[id.index] = fn.blocks.size();
    fn.blocks.push_back(std::move(block));
    return id;
  }

  void setInsertPoint(BlockId id) {
    currentBlockId = id;
  }

  BlockId getInsertPoint() const { return currentBlockId; }

  BasicBlock *currentBlock() {
    return fn.getBlock(currentBlockId);
  }

  bool currentBlockTerminated() const {
    const auto *b = fn.getBlock(currentBlockId);
    return b && b->hasTerminator();
  }

  ValueId allocateValueId() {
    return ValueId{nextValueIndex++};
  }

  void emitInstruction(CFGInstruction inst) {
    auto *bb = currentBlock();
    if (!bb)
      throw std::runtime_error("CFGBuilder: insert point not set");
    if (bb->hasTerminator())
      return; // Never emit instructions after terminator
    bb->instructions.push_back(std::move(inst));
  }

  void terminateWithJump(BlockId target) {
    auto *bb = currentBlock();
    if (!bb)
      throw std::runtime_error("CFGBuilder: insert point not set");
    if (bb->hasTerminator())
      return;
    bb->terminator = JumpTerminator{target};
  }

  void terminateWithBranch(CFGOperand cond, BlockId thenTarget,
                           BlockId elseTarget) {
    auto *bb = currentBlock();
    if (!bb)
      throw std::runtime_error("CFGBuilder: insert point not set");
    if (bb->hasTerminator())
      return;
    bb->terminator = BranchTerminator{std::move(cond), thenTarget, elseTarget};
  }

  void terminateWithReturn(std::optional<CFGOperand> val = std::nullopt) {
    auto *bb = currentBlock();
    if (!bb)
      throw std::runtime_error("CFGBuilder: insert point not set");
    if (bb->hasTerminator())
      return;
    bb->terminator = ReturnTerminator{std::move(val)};
  }

  void pushLoop(BlockId continueTarget, BlockId breakTarget) {
    loopStack.push_back(LoopContext{continueTarget, breakTarget});
  }

  void popLoop() {
    if (!loopStack.empty())
      loopStack.pop_back();
  }

  const LoopContext *currentLoop() const {
    if (loopStack.empty())
      return nullptr;
    return &loopStack.back();
  }

  void finalizeEdges() {
    // Clear existing edges
    for (auto &b : fn.blocks) {
      b->predecessors.clear();
      b->successors.clear();
    }

    // Populate successors from terminators
    for (auto &b : fn.blocks) {
      if (!b->terminator)
        continue;

      std::visit([&b](auto &&term) {
        using T = std::decay_t<decltype(term)>;
        if constexpr (std::is_same_v<T, JumpTerminator>) {
          b->successors.push_back(term.target);
        } else if constexpr (std::is_same_v<T, BranchTerminator>) {
          b->successors.push_back(term.thenTarget);
          if (term.elseTarget != term.thenTarget) {
            b->successors.push_back(term.elseTarget);
          }
        }
      }, *b->terminator);
    }

    // Populate predecessors from successors
    for (const auto &b : fn.blocks) {
      for (const auto &succId : b->successors) {
        if (auto *succBlock = fn.getBlock(succId)) {
          if (std::find(succBlock->predecessors.begin(),
                        succBlock->predecessors.end(),
                        b->id) == succBlock->predecessors.end()) {
            succBlock->predecessors.push_back(b->id);
          }
        }
      }
    }
  }
};

} // namespace nanopass
