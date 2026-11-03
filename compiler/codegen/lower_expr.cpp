#include "ast/expr.h"
#include "codegen/llvm_codegen.h"

using namespace llvm;

/* convert any value to i1 bool */
static Value *toBool(LLVMCodegen &cg, Value *v) {
  if (v->getType()->isIntegerTy(1))
    return v;
  if (v->getType()->isIntegerTy())
    return cg.builder.CreateICmpNE(v, ConstantInt::get(v->getType(), 0));
  if (v->getType()->isFloatingPointTy())
    return cg.builder.CreateFCmpONE(v, ConstantFP::get(v->getType(), 0.0));
  return v;
}

/* widen i1 to i32 for use in arithmetic contexts */
static Value *fromBool(LLVMCodegen &cg, Value *v) {
  if (v->getType()->isIntegerTy(1))
    return cg.builder.CreateZExt(v, Type::getInt32Ty(cg.ctx));
  return v;
}

/* ===== BOUNDS CHECK ===== */

static void emitBoundsCheck(LLVMCodegen &cg, Value *index, int size) {

  Function *fn = cg.currentFunction;

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
    return ConstantInt::get(Type::getInt1Ty(cg.ctx), b->value ? 1 : 0);

  /* ===== STRING ===== */
  if (auto *s = dynamic_cast<StringExpr *>(e))
    return cg.builder.CreateGlobalStringPtr(s->value);

  /* ===== VARIABLE ===== */
  if (auto *v = dynamic_cast<VariableExpr *>(e)) {
    VarInfo *info = cg.lookupVar(v->name);
    if (!info)
      llvm_unreachable("undefined variable in lowerExpr");
    return cg.builder.CreateLoad(cg.toLLVMType(info->type), info->slot);
  }

  /* ===== ARRAY ACCESS ===== */
  if (auto *a = dynamic_cast<IndexExpr *>(e)) {
    auto *var = dynamic_cast<VariableExpr *>(a->array.get());
    if (!var)
      llvm_unreachable("array base must be a variable");

    VarInfo *info = cg.lookupVar(var->name);
    if (!info)
      llvm_unreachable("undefined array in lowerExpr");

    Value *index = lowerExpr(cg, a->index.get());
    emitBoundsCheck(cg, index, info->type.arraySize);

    Value *zero = ConstantInt::get(Type::getInt32Ty(cg.ctx), 0);
    Value *ptr = cg.builder.CreateGEP(cg.toLLVMType(info->type), info->slot,
                                      {zero, index});
    return cg.builder.CreateLoad(cg.toLLVMType(*info->type.element), ptr);
  }

  /* ===== UNARY ===== */
  if (auto *u = dynamic_cast<UnaryExpr *>(e)) {
    Value *R = lowerExpr(cg, u->right.get());
    if (u->op == "-") {
      if (R->getType()->isFloatingPointTy())
        return cg.builder.CreateFNeg(R);
      return cg.builder.CreateNeg(R);
    }
    if (u->op == "!") {
      Value *b = toBool(cg, R);
      return cg.builder.CreateNot(b);
    }
    llvm_unreachable("unknown unary op");
  }

  /* ===== BINARY ===== */
  if (auto *b = dynamic_cast<BinaryExpr *>(e)) {

    /* ---- ASSIGNMENT ---- */
    if (b->op == "=") {
      if (auto *lhs = dynamic_cast<VariableExpr *>(b->left.get())) {
        Value *rhs = lowerExpr(cg, b->right.get());
        VarInfo *info = cg.lookupVar(lhs->name);
        if (!info)
          llvm_unreachable("assignment to undeclared variable");
        Type *declType = cg.toLLVMType(info->type);
        // Coerce: int->double, i1->i32
        if (rhs->getType() != declType) {
          if (rhs->getType()->isIntegerTy() && declType->isDoubleTy())
            rhs = cg.builder.CreateSIToFP(rhs, declType);
          else if (rhs->getType()->isIntegerTy(1) && declType->isIntegerTy(32))
            rhs = cg.builder.CreateZExt(rhs, declType);
        }
        cg.builder.CreateStore(rhs, info->slot);
        return rhs;
      }
      if (auto *a = dynamic_cast<IndexExpr *>(b->left.get())) {
        auto *var = dynamic_cast<VariableExpr *>(a->array.get());
        if (!var)
          llvm_unreachable("invalid array assignment");
        VarInfo *info = cg.lookupVar(var->name);
        if (!info)
          llvm_unreachable("undeclared array in assignment");
        Value *index = lowerExpr(cg, a->index.get());
        Value *rhs = lowerExpr(cg, b->right.get());
        Value *zero = ConstantInt::get(Type::getInt32Ty(cg.ctx), 0);
        Value *ptr = cg.builder.CreateGEP(cg.toLLVMType(info->type),
                                          info->slot, {zero, index});
        cg.builder.CreateStore(rhs, ptr);
        return rhs;
      }
      llvm_unreachable("Invalid assignment target in lowerExpr");
    }

    /* ---- SHORT-CIRCUIT LOGICAL ---- */
    if (b->op == "&&" || b->op == "||") {
      // Evaluate left; short-circuit right into a phi.
      Function *fn = cg.builder.GetInsertBlock()->getParent();
      Value *lv = toBool(cg, lowerExpr(cg, b->left.get()));
      BasicBlock *evalRHS = BasicBlock::Create(cg.ctx, "sc.rhs", fn);
      BasicBlock *merge   = BasicBlock::Create(cg.ctx, "sc.merge", fn);
      BasicBlock *lhsBB   = cg.builder.GetInsertBlock();

      if (b->op == "&&")
        cg.builder.CreateCondBr(lv, evalRHS, merge);
      else
        cg.builder.CreateCondBr(lv, merge, evalRHS);

      cg.builder.SetInsertPoint(evalRHS);
      Value *rv = toBool(cg, lowerExpr(cg, b->right.get()));
      BasicBlock *rhsBB = cg.builder.GetInsertBlock();
      cg.builder.CreateBr(merge);

      cg.builder.SetInsertPoint(merge);
      PHINode *phi = cg.builder.CreatePHI(Type::getInt1Ty(cg.ctx), 2);
      if (b->op == "&&") {
        phi->addIncoming(ConstantInt::getFalse(cg.ctx), lhsBB);
        phi->addIncoming(rv, rhsBB);
      } else {
        phi->addIncoming(ConstantInt::getTrue(cg.ctx), lhsBB);
        phi->addIncoming(rv, rhsBB);
      }
      return phi;
    }

    Value *L = lowerExpr(cg, b->left.get());
    Value *R = lowerExpr(cg, b->right.get());

    // Numeric promotions: i1 -> i32, int -> double
    if (L->getType()->isIntegerTy(1)) L = fromBool(cg, L);
    if (R->getType()->isIntegerTy(1)) R = fromBool(cg, R);
    if (L->getType()->isDoubleTy() && R->getType()->isIntegerTy())
      R = cg.builder.CreateSIToFP(R, L->getType());
    if (R->getType()->isDoubleTy() && L->getType()->isIntegerTy())
      L = cg.builder.CreateSIToFP(L, R->getType());

    Type *ty = L->getType();

    /* ---- FLOATING POINT ARITHMETIC ---- */
    if (ty->isFloatingPointTy()) {
      if (b->op == "+") return cg.builder.CreateFAdd(L, R);
      if (b->op == "-") return cg.builder.CreateFSub(L, R);
      if (b->op == "*") return cg.builder.CreateFMul(L, R);
      if (b->op == "/") return cg.builder.CreateFDiv(L, R);
      // Comparisons on floats -> i1
      if (b->op == "<")  return cg.builder.CreateFCmpOLT(L, R);
      if (b->op == "<=") return cg.builder.CreateFCmpOLE(L, R);
      if (b->op == ">")  return cg.builder.CreateFCmpOGT(L, R);
      if (b->op == ">=") return cg.builder.CreateFCmpOGE(L, R);
      if (b->op == "==") return cg.builder.CreateFCmpOEQ(L, R);
      if (b->op == "!=") return cg.builder.CreateFCmpONE(L, R);
    }

    /* ---- INTEGER ARITHMETIC ---- */
    if (ty->isIntegerTy()) {
      if (b->op == "+")  return cg.builder.CreateAdd(L, R);
      if (b->op == "-")  return cg.builder.CreateSub(L, R);
      if (b->op == "*")  return cg.builder.CreateMul(L, R);
      if (b->op == "/")  return cg.builder.CreateSDiv(L, R);
      if (b->op == "%")  return cg.builder.CreateSRem(L, R);
      // Comparisons on integers -> i1
      if (b->op == "<")  return cg.builder.CreateICmpSLT(L, R);
      if (b->op == "<=") return cg.builder.CreateICmpSLE(L, R);
      if (b->op == ">")  return cg.builder.CreateICmpSGT(L, R);
      if (b->op == ">=") return cg.builder.CreateICmpSGE(L, R);
      if (b->op == "==") return cg.builder.CreateICmpEQ(L, R);
      if (b->op == "!=") return cg.builder.CreateICmpNE(L, R);
    }

    llvm_unreachable("unhandled binary operator in lowerExpr");
  }

  /* ===== CALL ===== */
  if (auto *call = dynamic_cast<CallExpr *>(e)) {
    Function *fn = cg.module->getFunction(call->callee);
    if (!fn)
      llvm_unreachable("call to unknown function in lowerExpr");
    std::vector<Value *> args;
    for (auto &a : call->args)
      args.push_back(lowerExpr(cg, a.get()));
    return cg.builder.CreateCall(fn, args);
  }

  llvm_unreachable("unhandled expression kind in lowerExpr");
}
