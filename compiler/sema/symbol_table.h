#pragma once
#include <stdexcept>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../common/error.h"
#include "symbol.h"

using namespace std;

class SymbolTable {
  vector<unordered_map<string, Symbol *>> scopes;
  // AST nodes retain non-owning Symbol pointers. Keep the resolver's symbols
  // alive even after their lexical scope has been popped.
  vector<unique_ptr<Symbol>> ownedSymbols;

public:
  SymbolTable() {
    enterScope(); // global scope
  }

  // ---------------- Scope management ----------------

  void enterScope() { scopes.push_back({}); }

  void exitScope() {
    if (scopes.empty())
      throw CompileError("No scope to exit", 0, 0);
    scopes.pop_back();
  }

  int currentDepth() const { return (int)scopes.size() - 1; }

  // ---------------- Symbol operations ----------------

  void declare(const string &name, SymbolKind kind) {
    auto &scope = scopes.back();

    if (scope.count(name)) {
      throw CompileError("Redeclaration of symbol '" + name + "'", 0, 0);
    }

    ownedSymbols.push_back(make_unique<Symbol>(name, kind, currentDepth()));
    scope.emplace(name, ownedSymbols.back().get());
  }

  Symbol *lookup(const string &name) {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
      auto it = scopes[i].find(name);
      if (it != scopes[i].end())
        return it->second;
    }
    return nullptr;
  }

  bool isDeclaredInCurrentScope(const string &name) {
    return scopes.back().count(name);
  }
};
