#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "ir/anf/anf_ir.h"

namespace nanopass {

class ANFPrinter {
  std::ostream &out;
  int indentLevel = 0;

  void indent() {
    out << std::string(indentLevel * 2, ' ');
  }

public:
  explicit ANFPrinter(std::ostream &os) : out(os) {}

  void printProgram(const ANFProgram &program) {
    bool first = true;
    for (const auto &stmt : program.topLevelStmts) {
      printStmt(stmt.get());
    }

    for (const auto &fn : program.functions) {
      if (!first || !program.topLevelStmts.empty())
        out << "\n";
      printFunction(fn);
      first = false;
    }
  }

  void printFunction(const ANFFunction &fn) {
    indent();
    out << "func @" << fn.name << "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
      if (i > 0)
        out << ", ";
      out << fn.params[i].id.str() << " : " << typeName(fn.params[i].type);
    }
    out << ") -> " << typeName(fn.returnType) << " {\n";

    indentLevel++;
    for (const auto &stmt : fn.body) {
      printStmt(stmt.get());
    }
    indentLevel--;

    indent();
    out << "}\n";
  }

  void printStmt(ANFStmt *stmt) {
    if (!stmt)
      return;

    indent();

    if (auto *c = dynamic_cast<ANFComputeStmt *>(stmt)) {
      printComputation(c->comp);
      out << "\n";
      return;
    }

    if (auto *v = dynamic_cast<ANFVarDeclStmt *>(stmt)) {
      out << "var " << v->id.str() << " : " << typeName(v->type);
      if (v->init) {
        out << " = " << v->init->str();
      }
      out << "\n";
      return;
    }

    if (auto *a = dynamic_cast<ANFAssignStmt *>(stmt)) {
      out << a->target.str() << " = " << a->value.str() << "\n";
      return;
    }

    if (auto *ia = dynamic_cast<ANFIndexAssignStmt *>(stmt)) {
      out << ia->targetArray.str() << "[" << ia->index.str() << "] = "
          << ia->value.str() << "\n";
      return;
    }

    if (auto *i = dynamic_cast<ANFIfStmt *>(stmt)) {
      out << "if (" << i->condition.str() << ") {\n";
      indentLevel++;
      for (const auto &s : i->thenBranch)
        printStmt(s.get());
      indentLevel--;

      if (!i->elseBranch.empty()) {
        indent();
        out << "} else {\n";
        indentLevel++;
        for (const auto &s : i->elseBranch)
          printStmt(s.get());
        indentLevel--;
      }
      indent();
      out << "}\n";
      return;
    }

    if (auto *w = dynamic_cast<ANFWhileStmt *>(stmt)) {
      out << "while {\n";
      indentLevel++;
      for (const auto &s : w->condHeader)
        printStmt(s.get());
      indent();
      out << "if (" << w->condition.str() << ") {\n";
      indentLevel++;
      for (const auto &s : w->body)
        printStmt(s.get());
      indentLevel--;
      indent();
      out << "} else {\n";
      indentLevel++;
      indent();
      out << "break\n";
      indentLevel--;
      indent();
      out << "}\n";
      indentLevel--;
      indent();
      out << "}\n";
      return;
    }

    if (auto *r = dynamic_cast<ANFReturnStmt *>(stmt)) {
      out << "return";
      if (r->value) {
        out << " " << r->value->str();
      }
      out << "\n";
      return;
    }

    if (dynamic_cast<ANFBreakStmt *>(stmt)) {
      out << "break\n";
      return;
    }

    if (dynamic_cast<ANFContinueStmt *>(stmt)) {
      out << "continue\n";
      return;
    }

    out << "<unknown ANFStmt>\n";
  }

  void printComputation(const ANFComputation &comp) {
    if (comp.dst) {
      out << comp.dst->str() << " : " << typeName(comp.type) << " = ";
    }

    switch (comp.kind) {
    case ANFComputationKind::Binary:
      out << binaryOpToString(comp.binOp) << "(" << comp.left.str() << ", "
          << comp.right.str() << ")";
      break;
    case ANFComputationKind::Unary:
      out << unaryOpToString(comp.unOp) << "(" << comp.operand.str() << ")";
      break;
    case ANFComputationKind::Call:
      out << "call @" << comp.callee << "(";
      for (size_t i = 0; i < comp.args.size(); ++i) {
        if (i > 0)
          out << ", ";
        out << comp.args[i].str();
      }
      out << ")";
      break;
    case ANFComputationKind::IndexLoad:
      out << "load " << comp.array.str() << "[" << comp.index.str() << "]";
      break;
    }
  }

private:
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

inline void printANF(const ANFProgram &program, std::ostream &os) {
  ANFPrinter printer(os);
  printer.printProgram(program);
}

} // namespace nanopass
