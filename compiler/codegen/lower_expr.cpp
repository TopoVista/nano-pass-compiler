#include "ast/expr.h"
#include "codegen/llvm_codegen.h"

using namespace llvm;

/* convert to bool */
static Value *toBool(LLVMCodegen &cg, Value *v) {

  if (v->getType()->isIntegerTy(1))
    return v;

  if (v->getType()->isIntegerTy())
    return cg.builder.CreateICmpNE(v, ConstantInt::get(v->getType(), 0));

  if (v->getType()->isFloatingPointTy())
    return cg.builder.CreateFCmpONE(v, ConstantFP::get(v->getType(), 0.0));

  return v;
}

/* ===== BOUNDS CHECK ===== */

static void emitBoundsCheck(LLVMCodegen &cg, Value *index, int size) {

  Function *fn = cg.currentFunction;

  BasicBlock *currentBB = cg.builder.GetInsertBlock();

  Value *zero = ConstantInt::get(index->getType(), 0);
  Value *upper = ConstantInt::get(index->getType(), size);

  Value *lowerCheck = cg.builder.CreateICmpSGE(index, zero);
  Value *upperCheck = cg.builder.CreateICmpSLT(index, upper);
  Value *cond = cg.builder.CreateAnd(lowerCheck, upperCheck);

  BasicBlock *okBB = BasicBlock::Create(cg.ctx, "bounds.ok", fn);
  BasicBlock *errBB = BasicBlock::Create(cg.ctx, "bounds.err", fn);

  cg.builder.CreateCondBr(cond, okBB, errBB);

  // ----- ERROR BLOCK -----
  cg.builder.SetInsertPoint(errBB);

  cg.builder.CreateCall(cg.getPrintf(), {cg.builder.CreateGlobalStringPtr(
                                            "Array index out of bounds\n")});

  if (fn->getReturnType()->isVoidTy()) {
    cg.builder.CreateRetVoid();
  } else {
    cg.builder.CreateRet(Constant::getNullValue(fn->getReturnType()));
  }

  // ----- OK BLOCK -----
  cg.builder.SetInsertPoint(okBB);
}

Value *lowerExpr(LLVMCodegen &cg, Expr *e) {

  /* ===== NUMBER ===== */
  if (auto *n = dynamic_cast<NumberExpr *>(e)) {

    if (n->isFloat)
      return ConstantFP::get(Type::getDoubleTy(cg.ctx), n->floatValue);

    return ConstantInt::get(Type::getInt32Ty(cg.ctx), n->intValue);
  }

  /* ===== BOOL ===== */
  if (auto *b = dynamic_cast<BoolExpr *>(e))
    return ConstantInt::get(Type::getInt1Ty(cg.ctx), b->value);

  /* ===== VARIABLE ===== */
  if (auto *v = dynamic_cast<VariableExpr *>(e)) {

    VarInfo *info = cg.lookupVar(v->name);
    if (!info)
      llvm_unreachable("undefined variable");

    return cg.builder.CreateLoad(cg.toLLVMType(info->type), info->slot);
  }

  /* ===== ARRAY ACCESS ===== */
  if (auto *a = dynamic_cast<IndexExpr *>(e)) {

    auto *var = dynamic_cast<VariableExpr *>(a->array.get());

    if (!var)
      llvm_unreachable("array base must be variable");

    VarInfo *info = cg.lookupVar(var->name);
    if (!info)
      llvm_unreachable("undefined array");

    Value *index = lowerExpr(cg, a->index.get());

    emitBoundsCheck(cg, index, info->type.arraySize);

    Value *zero = ConstantInt::get(Type::getInt32Ty(cg.ctx), 0);

    Value *ptr = cg.builder.CreateGEP(cg.toLLVMType(info->type), info->slot,
                                      {zero, index});

    return cg.builder.CreateLoad(cg.toLLVMType(*info->type.element), ptr);
  }

  /* ===== BINARY ===== */
  if (auto *b = dynamic_cast<BinaryExpr *>(e)) {

    /* ASSIGNMENT */
    if (b->op == "=") {

      // -------- variable assignment --------
      if (auto *lhs = dynamic_cast<VariableExpr *>(b->left.get())) {

        Value *rhs = lowerExpr(cg, b->right.get());

        VarInfo *info = cg.lookupVar(lhs->name);
        if (!info)
          llvm_unreachable("assignment to undeclared");

        Type *declType = cg.toLLVMType(info->type);

        if (rhs->getType() != declType) {
          if (rhs->getType()->isIntegerTy() && declType->isDoubleTy())
            rhs = cg.builder.CreateSIToFP(rhs, declType);
        }

        cg.builder.CreateStore(rhs, info->slot);
        return rhs;
      }

      // -------- array element assignment --------
      if (auto *a = dynamic_cast<IndexExpr *>(b->left.get())) {

        auto *var = dynamic_cast<VariableExpr *>(a->array.get());
        if (!var)
          llvm_unreachable("invalid array assignment");

        VarInfo *info = cg.lookupVar(var->name);
        if (!info)
          llvm_unreachable("undeclared array");

        Value *index = lowerExpr(cg, a->index.get());
        Value *rhs = lowerExpr(cg, b->right.get());

        Value *zero = ConstantInt::get(Type::getInt32Ty(cg.ctx), 0);

        Value *ptr =
            cg.builder.CreateGEP(cg.toLLVMType(info->type), // FULL ARRAY TYPE
                                 info->slot, {zero, index});

        cg.builder.CreateStore(rhs, ptr);
        return rhs;
      }

      llvm_unreachable("Invalid assignment target");
    }

    Value *L = lowerExpr(cg, b->left.get());
    Value *R = lowerExpr(cg, b->right.get());

    if (L->getType()->isDoubleTy() && R->getType()->isIntegerTy())
      R = cg.builder.CreateSIToFP(R, L->getType());

    if (R->getType()->isDoubleTy() && L->getType()->isIntegerTy())
      L = cg.builder.CreateSIToFP(L, R->getType());

    Type *type = L->getType();

    if (type->isFloatingPointTy()) {

      if (b->op == "+")
        return cg.builder.CreateFAdd(L, R);
      if (b->op == "-")
        return cg.builder.CreateFSub(L, R);
      if (b->op == "*")
        return cg.builder.CreateFMul(L, R);
      if (b->op == "/")
        return cg.builder.CreateFDiv(L, R);
    }

    if (type->isIntegerTy()) {

      if (b->op == "+")
        return cg.builder.CreateAdd(L, R);
      if (b->op == "-")
        return cg.builder.CreateSub(L, R);
      if (b->op == "*")
        return cg.builder.CreateMul(L, R);
      if (b->op == "/")
        return cg.builder.CreateSDiv(L, R);
    }
  }

  /* ===== CALL ===== */
  if (auto *call = dynamic_cast<CallExpr *>(e)) {

    Function *fn = cg.module->getFunction(call->callee);

    std::vector<Value *> args;
    for (auto &a : call->args)
      args.push_back(lowerExpr(cg, a.get()));

    return cg.builder.CreateCall(fn, args);
  }

  llvm_unreachable("unhandled expr");
}
