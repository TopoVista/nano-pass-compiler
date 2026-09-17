#pragma once

#include <cstdint>
#include <string>

#include "ir/cfg/cfg_ir.h"

namespace nanopass {

struct CFGStats {
  size_t functions = 0;
  size_t basicBlocks = 0;
  size_t edges = 0;
  size_t instructions = 0;
  size_t branches = 0;
  size_t calls = 0;
  size_t returns = 0;
};

class CFGStatsAnalysis {
public:
  static CFGStats run(const CFGProgram &prog) {
    CFGStats stats;
    stats.functions = prog.functions.size();

    for (const auto &fn : prog.functions) {
      stats.basicBlocks += fn.blocks.size();
      for (const auto &b : fn.blocks) {
        stats.edges += b->successors.size();
        stats.instructions += b->instructions.size();

        for (const auto &inst : b->instructions) {
          if (inst.kind == CFGInstructionKind::Call)
            stats.calls++;
        }

        if (b->terminator) {
          std::visit([&stats](auto &&t) {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, BranchTerminator>) {
              stats.branches++;
            } else if constexpr (std::is_same_v<T, ReturnTerminator>) {
              stats.returns++;
            }
          }, *b->terminator);
        }
      }
    }

    return stats;
  }
};

} // namespace nanopass
