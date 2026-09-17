#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "ir/cfg/cfg_ir.h"

namespace nanopass {

class CFGGraphviz {
  std::ostream &out;

public:
  explicit CFGGraphviz(std::ostream &os) : out(os) {}

  void emit(const CFGProgram &program) {
    out << "digraph CFG {\n";
    out << "  node [shape=box, fontname=\"Courier\", style=\"rounded\"];\n";
    out << "  edge [fontname=\"Courier\"];\n\n";

    for (const auto &fn : program.functions) {
      emitFunction(fn);
    }

    out << "}\n";
  }

private:
  void emitFunction(const CFGFunction &fn) {
    out << "  subgraph cluster_" << fn.name << " {\n";
    out << "    label=\"@" << fn.name << "\";\n";
    out << "    color=blue;\n\n";

    for (const auto &b : fn.blocks) {
      std::string nodeId = fn.name + "_" + b->id.str();
      std::string labelText = b->id.str();
      if (!b->label.empty())
        labelText += " (" + b->label + ")";

      out << "    " << nodeId << " [label=\"" << labelText << "\"];\n";
    }

    out << "\n";

    for (const auto &b : fn.blocks) {
      if (!b->terminator)
        continue;

      std::string fromId = fn.name + "_" + b->id.str();

      std::visit([this, &fn, &fromId](auto &&term) {
        using T = std::decay_t<decltype(term)>;
        if constexpr (std::is_same_v<T, JumpTerminator>) {
          std::string toId = fn.name + "_" + term.target.str();
          out << "    " << fromId << " -> " << toId << ";\n";
        } else if constexpr (std::is_same_v<T, BranchTerminator>) {
          std::string thenId = fn.name + "_" + term.thenTarget.str();
          std::string elseId = fn.name + "_" + term.elseTarget.str();
          out << "    " << fromId << " -> " << thenId << " [label=\"true\", color=\"green\"];\n";
          out << "    " << fromId << " -> " << elseId << " [label=\"false\", color=\"red\"];\n";
        }
      }, *b->terminator);
    }

    out << "  }\n\n";
  }
};

inline void emitCFGGraphviz(const CFGProgram &program, std::ostream &os) {
  CFGGraphviz viz(os);
  viz.emit(program);
}

} // namespace nanopass
