#pragma once

#include "codegen/llvm_codegen.h"
#include "ast/stmt.h"

void lowerStmt(LLVMCodegen &cg, Stmt *stmt);
void lowerIfStmt(LLVMCodegen &cg, IfStmt *stmt);
void lowerWhileStmt(LLVMCodegen &cg, WhileStmt *stmt);
void lowerPrintStmt(LLVMCodegen &cg, PrintStmt *stmt);
void lowerExprStmt(LLVMCodegen &cg, ExprStmt *stmt);
