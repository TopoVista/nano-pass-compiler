#pragma once

#include <ostream>
#include <string>

#include "ir/cfg/analysis/dominator_analysis.h"

namespace nanopass {

inline void emitDominatorTreeGraphviz(const CFGProgram &program,
                                      std::ostream &out) {
  out << "digraph DominatorTree {\n"
      << "  node [shape=box, fontname=\"Courier\", style=\"rounded\"];\n\n";
  for (const auto &function : program.functions) {
    const DominatorTree dominators = DominatorAnalysis::run(function);
    out << "  subgraph cluster_dom_" << function.name << " {\n"
        << "    label=\"@" << function.name << " dominator tree\";\n"
        << "    color=darkgreen;\n";
    for (const auto &block : function.blocks) {
      if (!dominators.isReachable(block->id))
        continue;
      out << "    " << function.name << "_" << block->id.str()
          << " [label=\"" << block->id.str();
      if (!block->label.empty())
        out << " (" << block->label << ")";
      out << "\"];\n";
    }
    for (const auto &block : function.blocks) {
      const auto idom = dominators.immediateDominator(block->id);
      if (idom)
        out << "    " << function.name << "_" << idom->str() << " -> "
            << function.name << "_" << block->id.str() << ";\n";
    }
    out << "  }\n\n";
  }
  out << "}\n";
}

} // namespace nanopass
