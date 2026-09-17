#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "ir/core/core_ir.h"

namespace nanopass {

class CorePrinter {
  std::ostream &out;
  int indentLevel = 0;

  void indent() {
    out << std::string(indentLevel * 2, ' ');
  }

public:
  explicit CorePrinter(std::ostream &os) : out(os) {}

  void printProgram(const CoreProgram &program) {
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

  void printFunction(const CoreFunction &fn) {
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

  void printStmt(CoreStmt *stmt) {
    if (!stmt)
      return;

    indent();

    if (auto *v = dynamic_cast<CoreVarDeclStmt *>(stmt)) {
      out << "var " << v->id.str() << " : " << typeName(v->type);
      if (v->initializer) {
        out << " = ";
        printExpr(v->initializer.get());
      }
      out << "\n";
      return;
    }

    if (auto *a = dynamic_cast<CoreAssignStmt *>(stmt)) {
      out << a->target.str() << " = ";
      printExpr(a->value.get());
      out << "\n";
      return;
    }

    if (auto *ia = dynamic_cast<CoreIndexAssignStmt *>(stmt)) {
      out << ia->targetArray.str() << "[";
      printExpr(ia->index.get());
      out << "] = ";
      printExpr(ia->value.get());
      out << "\n";
      return;
    }

    if (auto *e = dynamic_cast<CoreExprStmt *>(stmt)) {
      printExpr(e->expr.get());
      out << "\n";
      return;
    }

    if (auto *i = dynamic_cast<CoreIfStmt *>(stmt)) {
      out << "if (";
      printExpr(i->condition.get());
      out << ") {\n";
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

    if (auto *w = dynamic_cast<CoreWhileStmt *>(stmt)) {
      out << "while (";
      printExpr(w->condition.get());
      out << ") {\n";
      indentLevel++;
      for (const auto &s : w->body)
        printStmt(s.get());
      indentLevel--;
      indent();
      out << "}\n";
      return;
    }

    if (auto *r = dynamic_cast<CoreReturnStmt *>(stmt)) {
      out << "return";
      if (r->value) {
        out << " ";
        printExpr(r->value.get());
      }
      out << "\n";
      return;
    }

    if (dynamic_cast<CoreBreakStmt *>(stmt)) {
      out << "break\n";
      return;
    }

    if (dynamic_cast<CoreContinueStmt *>(stmt)) {
      out << "continue\n";
      return;
    }

    out << "<unknown CoreStmt>\n";
  }

  void printExpr(CoreExpr *expr) {
    if (!expr) {
      out << "<null>";
      return;
    }

    if (auto *c = dynamic_cast<CoreConstantExpr *>(expr)) {
      std::visit([this](auto &&val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) {
          out << (val ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::string>) {
          out << "\"" << val << "\"";
        } else {
          out << val;
        }
      }, c->value);
      return;
    }

    if (auto *v = dynamic_cast<CoreVariableRefExpr *>(expr)) {
      out << v->id.str();
      return;
    }

    if (auto *idx = dynamic_cast<CoreIndexExpr *>(expr)) {
      printExpr(idx->array.get());
      out << "[";
      printExpr(idx->index.get());
      out << "]";
      return;
    }

    if (auto *u = dynamic_cast<CoreUnaryExpr *>(expr)) {
      out << unaryOpToString(u->op) << "(";
      printExpr(u->operand.get());
      out << ")";
      return;
    }

    if (auto *b = dynamic_cast<CoreBinaryExpr *>(expr)) {
      out << binaryOpToString(b->op) << "(";
      printExpr(b->left.get());
      out << ", ";
      printExpr(b->right.get());
      out << ")";
      return;
    }

    if (auto *call = dynamic_cast<CoreCallExpr *>(expr)) {
      out << "call @" << call->callee << "(";
      for (size_t i = 0; i < call->args.size(); ++i) {
        if (i > 0)
          out << ", ";
        printExpr(call->args[i].get());
      }
      out << ")";
      return;
    }

    out << "<unknown CoreExpr>";
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

inline void printCoreIR(const CoreProgram &program, std::ostream &os) {
  CorePrinter printer(os);
  printer.printProgram(program);
}

} // namespace nanopass
