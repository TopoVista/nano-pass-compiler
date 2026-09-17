#pragma once

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "ir/cfg/cfg_ir.h"

namespace nanopass {

struct RPOResult {
  std::vector<BlockId> order;
};

class ReversePostOrderAnalysis {
public:
  static RPOResult run(const CFGFunction &fn) {
    RPOResult res;
    std::unordered_set<int> visited;
    std::vector<BlockId> postOrder;

    if (fn.getBlock(fn.entryBlock)) {
      dfs(fn, fn.entryBlock, visited, postOrder);
    }

    std::reverse(postOrder.begin(), postOrder.end());
    res.order = std::move(postOrder);
    return res;
  }

private:
  static void dfs(const CFGFunction &fn, BlockId currId,
                  std::unordered_set<int> &visited,
                  std::vector<BlockId> &postOrder) {
    visited.insert(currId.index);
    const auto *bb = fn.getBlock(currId);
    if (!bb)
      return;

    for (const auto &succ : bb->successors) {
      if (!visited.count(succ.index)) {
        dfs(fn, succ, visited, postOrder);
      }
    }

    postOrder.push_back(currId);
  }
};

} // namespace nanopass
