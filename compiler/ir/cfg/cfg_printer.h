#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "ir/cfg/cfg_ir.h"

namespace nanopass {

class CFGPrinter {
  std::ostream &out;

public:
  explicit CFGPrinter(std::ostream &os) : out(os) {}

  void printProgram(const CFGProgram &program) {
    bool first = true;
    for (const auto &fn : program.functions) {
      if (!first)
        out << "\n";
      printFunction(fn);
      first = false;
    }
  }

  void printFunction(const CFGFunction &fn) {
    out << "func @" << fn.name << "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
      if (i > 0)
        out << ", ";
      out << fn.params[i].id.str() << " : " << typeName(fn.params[i].type);
    }
    out << ") -> " << typeName(fn.returnType) << " {\n";

    for (const auto &b : fn.blocks) {
      printBlock(*b);
    }

    out << "}\n";
  }

  void printBlock(const BasicBlock &b) {
    out << b.id.str();
    if (!b.label.empty())
      out << "." << b.label;
    out << ":\n";

    for (const auto &inst : b.instructions) {
      out << "  ";
      printInstruction(inst);
      out << "\n";
    }

    if (b.terminator) {
      out << "  ";
      printTerminator(*b.terminator);
      out << "\n";
    }
  }

  void printInstruction(const CFGInstruction &inst) {
    switch (inst.kind) {
    case CFGInstructionKind::Constant:
      if (inst.dst)
        out << inst.dst->str() << " = const ";
      printConstant(inst.constantVal);
      break;

    case CFGInstructionKind::Copy:
      if (inst.dst)
        out << inst.dst->str() << " = copy " << inst.op1.str();
      break;

    case CFGInstructionKind::Unary:
      if (inst.dst)
        out << inst.dst->str() << " = " << unaryOpToString(inst.unOp) << " "
            << inst.op1.str();
      break;

    case CFGInstructionKind::Binary:
      if (inst.dst)
        out << inst.dst->str() << " = " << binaryOpToString(inst.binOp) << " "
            << inst.op1.str() << ", " << inst.op2.str();
      break;

    case CFGInstructionKind::Call:
      if (inst.dst)
        out << inst.dst->str() << " = ";
      out << "call @" << inst.callee << "(";
      for (size_t i = 0; i < inst.callArgs.size(); ++i) {
        if (i > 0)
          out << ", ";
        out << inst.callArgs[i].str();
      }
      out << ")";
      break;

    case CFGInstructionKind::LoadLocal:
      if (inst.dst)
        out << inst.dst->str() << " = load " << inst.local.str();
      break;

    case CFGInstructionKind::StoreLocal:
      out << "store " << inst.op1.str() << " -> " << inst.local.str();
      break;

    case CFGInstructionKind::LoadIndex:
      if (inst.dst)
        out << inst.dst->str() << " = load " << inst.local.str() << "["
            << inst.op1.str() << "]";
      break;

    case CFGInstructionKind::StoreIndex:
      out << "store " << inst.op2.str() << " -> " << inst.local.str() << "["
          << inst.op1.str() << "]";
      break;
    }
  }

  void printTerminator(const Terminator &term) {
    std::visit([this](auto &&t) {
      using T = std::decay_t<decltype(t)>;
      if constexpr (std::is_same_v<T, JumpTerminator>) {
        out << "jump " << t.target.str();
      } else if constexpr (std::is_same_v<T, BranchTerminator>) {
        out << "branch " << t.condition.str() << ", " << t.thenTarget.str()
            << ", " << t.elseTarget.str();
      } else if constexpr (std::is_same_v<T, ReturnTerminator>) {
        out << "return";
        if (t.value) {
          out << " " << t.value->str();
        }
      } else if constexpr (std::is_same_v<T, UnreachableTerminator>) {
        out << "unreachable";
      }
    }, term);
  }

private:
  void printConstant(const CoreConstantVal &val) {
    std::visit([this](auto &&v) {
      using T = std::decay_t<decltype(v)>;
      if constexpr (std::is_same_v<T, bool>) {
        out << (v ? "true" : "false");
      } else if constexpr (std::is_same_v<T, std::string>) {
        out << "\"" << v << "\"";
      } else {
        out << v;
      }
    }, val);
  }

  static std::string typeName(const LangType &t) {
    switch (t.kind) {
    case LangTypeKind::Integer:
      return (t.bitWidth == 64 ? "i64" : (t.bitWidth == 8 ? "i8" : "int"));
    case LangTypeKind::Floating:
      return "float";
    case LangTypeKind::Bool:
      return "bool";
    case LangTypeKind::Char:
      return "char";
    case LangTypeKind::String:
      return "string";
    case LangTypeKind::Array:
      return "array";
    case LangTypeKind::Function:
      return "function";
    case LangTypeKind::Void:
      return "void";
    case LangTypeKind::Unknown:
      return "unknown";
    }
    return "unknown";
  }
};

inline void printCFG(const CFGProgram &program, std::ostream &os) {
  CFGPrinter printer(os);
  printer.printProgram(program);
}

} // namespace nanopass
