#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "../sema/type.h"

struct VarInfo {
  LangType type;
  llvm::Value *slot;
};

struct LLVMCodegen {
  llvm::LLVMContext &ctx;
  llvm::Module *module;
  llvm::IRBuilder<> builder;

  llvm::Function *currentFunction = nullptr;

  std::vector<std::unordered_map<std::string, VarInfo>> scopes;

  LLVMCodegen(llvm::LLVMContext &c, llvm::Module *m)
      : ctx(c), module(m), builder(c) {
    scopes.emplace_back();
  }

  void enterScope() { scopes.emplace_back(); }

  void exitScope() { scopes.pop_back(); }

  void bind(const std::string &name, const LangType &type, llvm::Value *slot) {
    scopes.back()[name] = VarInfo{type, slot};
  }

  VarInfo *lookupVar(const std::string &name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto f = it->find(name);
      if (f != it->end())
        return &f->second;
    }
    return nullptr;
  }

  LangType *lookupType(const std::string &name) {
    VarInfo *info = lookupVar(name);
    return info ? &info->type : nullptr;
  }

  llvm::Function *getPrintf();
  void emitPrintfInt(llvm::Value *v);
  void emitPrintfFloat(llvm::Value *v);
  void emitPrintfBool(llvm::Value *v);
  void emitPrintfStr(llvm::Value *v);

  llvm::Type *toLLVMType(const LangType &type);
};
