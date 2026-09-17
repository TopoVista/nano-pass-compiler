#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "ir/cfg/analysis/dominator_analysis.h"

namespace nanopass {

class DominanceFrontier {
  std::unordered_map<int, std::vector<BlockId>> frontiers_;

public:
  const std::vector<BlockId> &frontier(BlockId block) const {
    static const std::vector<BlockId> empty;
    auto it = frontiers_.find(block.index);
    return it == frontiers_.end() ? empty : it->second;
  }

  bool contains(BlockId block, BlockId frontierBlock) const {
    const auto &set = frontier(block);
    return std::binary_search(set.begin(), set.end(), frontierBlock);
  }

private:
  friend class DominanceFrontierAnalysis;
};

// Computes the standard local dominance frontier from immediate dominators.
// Each result set is sorted by BlockId, so phi placement and diagnostics are
// deterministic regardless of hash-table iteration order.
class DominanceFrontierAnalysis {
public:
  static DominanceFrontier run(const CFGFunction &function,
                               const DominatorTree &dominators) {
    DominanceFrontier result;
    const auto rpo = ReversePostOrderAnalysis::run(function).order;
    for (BlockId join : rpo) {
      const BasicBlock *block = function.getBlock(join);
      if (!block || block->predecessors.size() < 2)
        continue;
      const auto joinIdom = dominators.immediateDominator(join);
      if (!joinIdom)
        continue;

      for (BlockId predecessor : block->predecessors) {
        if (!dominators.isReachable(predecessor))
          continue;
        BlockId runner = predecessor;
        while (runner != *joinIdom) {
          auto &set = result.frontiers_[runner.index];
          if (std::find(set.begin(), set.end(), join) == set.end())
            set.push_back(join);
          const auto runnerIdom = dominators.immediateDominator(runner);
          if (!runnerIdom)
            break;
          runner = *runnerIdom;
        }
      }
    }
    for (auto &[_, set] : result.frontiers_) {
      std::sort(set.begin(), set.end());
      set.erase(std::unique(set.begin(), set.end()), set.end());
    }
    return result;
  }
};

} // namespace nanopass
