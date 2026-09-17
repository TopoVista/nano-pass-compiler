#pragma once

#include <algorithm>
#include <queue>
#include <unordered_set>
#include <vector>

#include "ir/cfg/cfg_ir.h"

namespace nanopass {

struct ReachabilityResult {
  std::vector<BlockId> reachable;
  std::vector<BlockId> unreachable;

  bool isReachable(BlockId id) const {
    return std::find(reachable.begin(), reachable.end(), id) != reachable.end();
  }
};

class ReachabilityAnalysis {
public:
  static ReachabilityResult run(const CFGFunction &fn) {
    ReachabilityResult res;
    std::unordered_set<int> visited;
    std::queue<BlockId> q;

    if (fn.getBlock(fn.entryBlock)) {
      q.push(fn.entryBlock);
      visited.insert(fn.entryBlock.index);
    }

    while (!q.empty()) {
      BlockId currId = q.front();
      q.pop();
      res.reachable.push_back(currId);

      const auto *bb = fn.getBlock(currId);
      if (!bb)
        continue;

      // Traversal uses deterministic successor list
      for (const auto &succ : bb->successors) {
        if (visited.insert(succ.index).second) {
          q.push(succ);
        }
      }
    }

    for (const auto &b : fn.blocks) {
      if (!visited.count(b->id.index)) {
        res.unreachable.push_back(b->id);
      }
    }

    return res;
  }
};

} // namespace nanopass
