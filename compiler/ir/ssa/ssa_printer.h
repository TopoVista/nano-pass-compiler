#pragma once

#include <ostream>
#include <type_traits>

#include "ir/ssa/ssa_ir.h"

namespace nanopass {

inline const char *ssaTypeName(const LangType &type) {
  switch (type.kind) {
  case LangTypeKind::Integer: return "int";
  case LangTypeKind::Floating: return "float";
  case LangTypeKind::Bool: return "bool";
  case LangTypeKind::Char: return "char";
  case LangTypeKind::String: return "string";
  case LangTypeKind::Array: return "array";
  case LangTypeKind::Void: return "void";
  default: return "unknown";
  }
}

inline void printSSAOperand(const SSAOperand &operand, std::ostream &out) {
  if (std::holds_alternative<ValueId>(operand)) out << std::get<ValueId>(operand).str();
  else std::visit([&](const auto &value) { out << value; }, std::get<CoreConstantVal>(operand));
}

inline void printSSAInstruction(const SSAInstruction &instruction, std::ostream &out) {
  if (instruction.result) out << instruction.result->str() << " = ";
  switch (instruction.kind) {
  case SSAInstructionKind::Constant:
    std::visit([&](const auto &value) { out << "const " << value; }, instruction.constantValue); break;
  case SSAInstructionKind::Copy:
    out << "copy "; printSSAOperand(instruction.operand1, out); break;
  case SSAInstructionKind::Unary:
    out << unaryOpToString(instruction.unaryOp) << ' '; printSSAOperand(instruction.operand1, out); break;
  case SSAInstructionKind::Binary:
    out << binaryOpToString(instruction.binaryOp) << ' '; printSSAOperand(instruction.operand1, out);
    out << ", "; printSSAOperand(instruction.operand2, out); break;
  case SSAInstructionKind::Call:
    out << "call @" << instruction.callee << '(';
    for (size_t i = 0; i < instruction.callArgs.size(); ++i) { if (i) out << ", "; printSSAOperand(instruction.callArgs[i], out); }
    out << ')'; break;
  case SSAInstructionKind::LoadLocal: out << "load.local " << instruction.local.str(); break;
  case SSAInstructionKind::StoreLocal: out << "store.local " << instruction.local.str() << ", "; printSSAOperand(instruction.operand1, out); break;
  case SSAInstructionKind::LoadIndex: out << "load.index " << instruction.local.str() << ", "; printSSAOperand(instruction.operand1, out); break;
  case SSAInstructionKind::StoreIndex: out << "store.index " << instruction.local.str() << ", "; printSSAOperand(instruction.operand1, out); out << ", "; printSSAOperand(instruction.operand2, out); break;
  }
}

inline void printSSA(const SSAProgram &program, std::ostream &out) {
  for (const auto &function : program.functions) {
    out << "func @" << function.name << '(';
    for (size_t i = 0; i < function.parameters.size(); ++i) { if (i) out << ", "; out << function.parameters[i].value.str() << " : " << ssaTypeName(function.parameters[i].type); }
    out << ") -> " << ssaTypeName(function.returnType) << " {\n";
    for (const auto &block : function.blocks) {
      out << block.id.str() << '.' << block.label << ":\n";
      for (const auto &phi : block.phis) {
        out << "  " << phi.result.str() << " = phi " << ssaTypeName(phi.type) << ' ';
        for (size_t i = 0; i < phi.incoming.size(); ++i) { if (i) out << ", "; out << '[' << phi.incoming[i].predecessor.str() << ": " << phi.incoming[i].value.str() << ']'; }
        out << " ; " << phi.originalLocal.str() << "\n";
      }
      for (const auto &instruction : block.instructions) { out << "  "; printSSAInstruction(instruction, out); out << "\n"; }
      std::visit([&](const auto &term) { using T = std::decay_t<decltype(term)>; out << "  "; if constexpr (std::is_same_v<T, SSATerminator::Jump>) out << "jump " << term.target.str(); else if constexpr (std::is_same_v<T, SSATerminator::Branch>) { out << "branch "; printSSAOperand(term.condition, out); out << ", " << term.thenTarget.str() << ", " << term.elseTarget.str(); } else if constexpr (std::is_same_v<T, SSATerminator::Return>) { out << "return"; if (term.value) { out << ' '; printSSAOperand(*term.value, out); } } else out << "unreachable"; out << "\n"; }, block.terminator.value);
    }
    out << "}\n";
  }
}

} // namespace nanopass
