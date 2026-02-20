#pragma once

#include "llvm_codegen.h"
#include "ast/expr.h"

llvm::Value *lowerExpr(LLVMCodegen &cg, Expr *expr);
