#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>

#include "symbol.h"

using namespace std;

class SymbolTable {
    vector<unordered_map<string, Symbol>> scopes;

public:
    SymbolTable() {
        enterScope(); // global scope
    }

    // ---------------- Scope management ----------------

    void enterScope() {
        scopes.push_back({});
    }

    void exitScope() {
        if (scopes.empty())
            throw runtime_error("No scope to exit");
        scopes.pop_back();
    }

    int currentDepth() const {
        return (int)scopes.size() - 1;
    }

    // ---------------- Symbol operations ----------------

    void declare(const string& name, SymbolKind kind) {
        auto& scope = scopes.back();
        if (scope.count(name))
            throw runtime_error("Redeclaration of symbol: " + name);

        scope.emplace(
            name,
            Symbol(name, kind, currentDepth())
        );
    }

    Symbol* lookup(const string& name) {
        for (int i = (int)scopes.size() - 1; i >= 0; --i) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end())
                return &it->second;
        }
        return nullptr;
    }

    bool isDeclaredInCurrentScope(const string& name) {
        return scopes.back().count(name);
    }
};
