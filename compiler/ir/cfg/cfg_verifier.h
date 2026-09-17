#pragma once

#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/cfg/cfg_ir.h"

namespace nanopass {

struct CFGVerifierResult {
  bool ok = true;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::vector<BlockId> reachableBlocks;
  std::vector<BlockId> unreachableBlocks;

  void addError(std::string err) {
    ok = false;
    errors.push_back(std::move(err));
  }

  void addWarning(std::string warn) {
    warnings.push_back(std::move(warn));
  }
};

class CFGVerifier {
  CFGVerifierResult result;

public:
  CFGVerifierResult verify(const CFGProgram &program) {
    result = CFGVerifierResult{};

    for (const auto &fn : program.functions) {
      verifyFunction(fn);
    }

    return result;
  }

private:
  void verifyFunction(const CFGFunction &fn) {
    // 1. Check entry block exists
    const BasicBlock *entry = fn.getBlock(fn.entryBlock);
    if (!entry) {
      result.addError("Function @" + fn.name + " has invalid entry block: " +
                      fn.entryBlock.str());
      return;
    }

    // Entry block should have no predecessors
    if (!entry->predecessors.empty()) {
      result.addError("Entry block " + entry->id.str() + " in @" + fn.name +
                      " has predecessors");
    }

    // 2. Check block ID uniqueness and mapping
    std::unordered_set<int> seenBlockIds;
    for (const auto &b : fn.blocks) {
      if (!seenBlockIds.insert(b->id.index).second) {
        result.addError("Duplicate block ID in @" + fn.name + ": " + b->id.str());
      }
    }

    // 3. Verify each block: exactly one terminator, target existence, and edge consistency
    for (const auto &b : fn.blocks) {
      if (!b->hasTerminator()) {
        result.addError("Block " + b->id.str() + " in @" + fn.name +
                        " is missing a terminator");
        continue;
      }

      std::visit([this, &fn, &b](auto &&term) {
        using T = std::decay_t<decltype(term)>;
        if constexpr (std::is_same_v<T, JumpTerminator>) {
          if (!fn.getBlock(term.target)) {
            result.addError("Jump in " + b->id.str() + " targets non-existent block: " +
                            term.target.str());
          }
        } else if constexpr (std::is_same_v<T, BranchTerminator>) {
          if (!fn.getBlock(term.thenTarget)) {
            result.addError("Branch 'then' in " + b->id.str() +
                            " targets non-existent block: " + term.thenTarget.str());
          }
          if (!fn.getBlock(term.elseTarget)) {
            result.addError("Branch 'else' in " + b->id.str() +
                            " targets non-existent block: " + term.elseTarget.str());
          }
        }
      }, *b->terminator);

      // Verify predecessor/successor consistency
      for (const auto &succId : b->successors) {
        const BasicBlock *succ = fn.getBlock(succId);
        if (succ) {
          bool foundPred = false;
          for (const auto &p : succ->predecessors) {
            if (p == b->id) {
              foundPred = true;
              break;
            }
          }
          if (!foundPred) {
            result.addError("Inconsistency: " + b->id.str() + " has successor " +
                            succId.str() + ", but " + succId.str() +
                            " does not list " + b->id.str() + " as predecessor");
          }
        }
      }

      for (const auto &predId : b->predecessors) {
        const BasicBlock *pred = fn.getBlock(predId);
        if (pred) {
          bool foundSucc = false;
          for (const auto &s : pred->successors) {
            if (s == b->id) {
              foundSucc = true;
              break;
            }
          }
          if (!foundSucc) {
            result.addError("Inconsistency: " + b->id.str() + " has predecessor " +
                            predId.str() + ", but " + predId.str() +
                            " does not list " + b->id.str() + " as successor");
          }
        }
      }
    }

    // 4. Reachability analysis (BFS from entry)
    std::unordered_set<int> visited;
    std::queue<BlockId> worklist;
    worklist.push(fn.entryBlock);
    visited.insert(fn.entryBlock.index);

    while (!worklist.empty()) {
      BlockId currId = worklist.front();
      worklist.pop();
      result.reachableBlocks.push_back(currId);

      const BasicBlock *curr = fn.getBlock(currId);
      if (!curr)
        continue;

      for (const auto &succId : curr->successors) {
        if (visited.insert(succId.index).second) {
          worklist.push(succId);
        }
      }
    }

    // Identify unreachable blocks
    for (const auto &b : fn.blocks) {
      if (!visited.count(b->id.index)) {
        result.unreachableBlocks.push_back(b->id);
        result.addWarning("Unreachable block " + b->id.str() + " in @" + fn.name);
      }
    }
  }
};

inline CFGVerifierResult verifyCFG(const CFGProgram &program) {
  CFGVerifier verifier;
  return verifier.verify(program);
}

} // namespace nanopass
