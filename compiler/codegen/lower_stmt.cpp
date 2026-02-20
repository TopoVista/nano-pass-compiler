#include "codegen/lower_stmt.h"
#include "codegen/lower_expr.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/ErrorHandling.h>

using namespace llvm;

/* ================= BLOCK ================= */

static void lowerBlock(LLVMCodegen &cg, BlockStmt *blk) {
  cg.enterScope();
  for (auto &s : blk->stmts) {
    if (cg.builder.GetInsertBlock()->getTerminator())
      break;
    lowerStmt(cg, s.get());
  }
  cg.exitScope();
}

/* ================= IF ================= */

void lowerIfStmt(LLVMCodegen &cg, IfStmt *stmt) {
  Function *fn = cg.builder.GetInsertBlock()->getParent();

  Value *condVal = lowerExpr(cg, stmt->condition.get());

  if (condVal->getType()->isIntegerTy())
    condVal = cg.builder.CreateICmpNE(
        condVal, ConstantInt::get(condVal->getType(), 0), "ifcond");

  BasicBlock *thenBB = BasicBlock::Create(cg.ctx, "then", fn);
  BasicBlock *elseBB =
      stmt->elseBranch ? BasicBlock::Create(cg.ctx, "else", fn) : nullptr;
  BasicBlock *mergeBB = BasicBlock::Create(cg.ctx, "ifcont", fn);

  if (elseBB)
    cg.builder.CreateCondBr(condVal, thenBB, elseBB);
  else
    cg.builder.CreateCondBr(condVal, thenBB, mergeBB);

  cg.builder.SetInsertPoint(thenBB);
  lowerStmt(cg, stmt->thenBranch.get());
  if (!cg.builder.GetInsertBlock()->getTerminator())
    cg.builder.CreateBr(mergeBB);

  if (elseBB) {
    cg.builder.SetInsertPoint(elseBB);
    lowerStmt(cg, stmt->elseBranch.get());
    if (!cg.builder.GetInsertBlock()->getTerminator())
      cg.builder.CreateBr(mergeBB);
  }

  cg.builder.SetInsertPoint(mergeBB);
}

/* ================= WHILE ================= */

void lowerWhileStmt(LLVMCodegen &cg, WhileStmt *stmt) {
  Function *fn = cg.builder.GetInsertBlock()->getParent();

  BasicBlock *condBB = BasicBlock::Create(cg.ctx, "while.cond", fn);
  BasicBlock *bodyBB = BasicBlock::Create(cg.ctx, "while.body", fn);
  BasicBlock *exitBB = BasicBlock::Create(cg.ctx, "while.exit", fn);

  cg.builder.CreateBr(condBB);

  cg.builder.SetInsertPoint(condBB);

  Value *condVal = lowerExpr(cg, stmt->condition.get());

  if (condVal->getType()->isIntegerTy())
    condVal = cg.builder.CreateICmpNE(
        condVal, ConstantInt::get(condVal->getType(), 0), "whilecond");

  cg.builder.CreateCondBr(condVal, bodyBB, exitBB);

  cg.builder.SetInsertPoint(bodyBB);
  lowerStmt(cg, stmt->body.get());
  if (!cg.builder.GetInsertBlock()->getTerminator())
    cg.builder.CreateBr(condBB);

  cg.builder.SetInsertPoint(exitBB);
}

/* ================= RETURN ================= */

void lowerReturnStmt(LLVMCodegen &cg, ReturnStmt *stmt) {
  if (stmt->value) {
    Value *retVal = lowerExpr(cg, stmt->value.get());
    cg.builder.CreateRet(retVal);
  } else {
    cg.builder.CreateRet(ConstantInt::get(Type::getInt32Ty(cg.ctx), 0));
  }
}

/* ================= FOR ================= */

void lowerForStmt(LLVMCodegen &cg, ForStmt *stmt) {

  cg.enterScope();

  if (stmt->init)
    lowerStmt(cg, stmt->init.get());

  Function *fn = cg.builder.GetInsertBlock()->getParent();

  BasicBlock *condBB = BasicBlock::Create(cg.ctx, "for.cond", fn);
  BasicBlock *bodyBB = BasicBlock::Create(cg.ctx, "for.body", fn);
  BasicBlock *incBB = BasicBlock::Create(cg.ctx, "for.inc", fn);
  BasicBlock *exitBB = BasicBlock::Create(cg.ctx, "for.exit", fn);

  cg.builder.CreateBr(condBB);
  cg.builder.SetInsertPoint(condBB);

  Value *condVal;

  if (stmt->condition) {
    condVal = lowerExpr(cg, stmt->condition.get());
    if (condVal->getType()->isIntegerTy())
      condVal = cg.builder.CreateICmpNE(
          condVal, ConstantInt::get(condVal->getType(), 0), "forcond");
  } else {
    condVal = ConstantInt::getTrue(cg.ctx);
  }

  cg.builder.CreateCondBr(condVal, bodyBB, exitBB);

  cg.builder.SetInsertPoint(bodyBB);
  lowerStmt(cg, stmt->body.get());
  if (!cg.builder.GetInsertBlock()->getTerminator())
    cg.builder.CreateBr(incBB);

  cg.builder.SetInsertPoint(incBB);
  if (stmt->increment)
    lowerExpr(cg, stmt->increment.get());

  cg.builder.CreateBr(condBB);

  cg.builder.SetInsertPoint(exitBB);

  cg.exitScope();
}

/* ================= FUNCTION ================= */

void lowerFunctionStmt(LLVMCodegen &cg, FunctionStmt *stmt) {

  Function *oldFunction = cg.currentFunction;
  BasicBlock *oldInsertBlock = cg.builder.GetInsertBlock();

  std::vector<Type *> paramTypes;
  for (auto &p : stmt->params)
    paramTypes.push_back(cg.toLLVMType(p.second));

  Type *retType = cg.toLLVMType(stmt->returnType);

  FunctionType *fnType = FunctionType::get(retType, paramTypes, false);

  Function *fn = Function::Create(fnType, Function::ExternalLinkage, stmt->name,
                                  cg.module);

  cg.currentFunction = fn;

  BasicBlock *entry = BasicBlock::Create(cg.ctx, "entry", fn);
  cg.builder.SetInsertPoint(entry);

  cg.enterScope();

  unsigned idx = 0;
  for (auto &arg : fn->args()) {

    auto &paramPair = stmt->params[idx++];
    const std::string &paramName = paramPair.first;
    LangType paramType = paramPair.second;

    IRBuilder<> tmp(&fn->getEntryBlock(), fn->getEntryBlock().begin());

    AllocaInst *slot = tmp.CreateAlloca(arg.getType(), nullptr, paramName);

    cg.builder.CreateStore(&arg, slot);

    cg.bind(paramName, paramType, slot);
  }

  lowerBlock(cg, stmt->body.get());

  if (!cg.builder.GetInsertBlock()->getTerminator()) {
    if (retType->isVoidTy())
      cg.builder.CreateRetVoid();
    else
      cg.builder.CreateRet(Constant::getNullValue(retType));
  }

  cg.exitScope();

  cg.currentFunction = oldFunction;

  if (oldInsertBlock)
    cg.builder.SetInsertPoint(oldInsertBlock);
}

/* ================= VAR DECL ================= */

void lowerVarDeclStmt(LLVMCodegen &cg, VarDeclStmt *stmt) {

  Type *llvmType = cg.toLLVMType(stmt->type);

  IRBuilder<> tmp(&cg.currentFunction->getEntryBlock(),
                  cg.currentFunction->getEntryBlock().begin());

  AllocaInst *slot = tmp.CreateAlloca(llvmType, nullptr, stmt->name);

  cg.bind(stmt->name, stmt->type, slot);

  if (stmt->initializer) {

    Value *initVal = lowerExpr(cg, stmt->initializer.get());

    if (initVal->getType() != llvmType) {

      if (initVal->getType()->isIntegerTy(32) && llvmType->isDoubleTy()) {
        initVal = cg.builder.CreateSIToFP(initVal, llvmType);
      } else {
        llvm_unreachable("Type mismatch in variable declaration");
      }
    }

    cg.builder.CreateStore(initVal, slot);
  }
}

/* ================= DISPATCH ================= */

void lowerStmt(LLVMCodegen &cg, Stmt *stmt) {

  if (auto *s = dynamic_cast<ExprStmt *>(stmt)) {
    lowerExpr(cg, s->e.get());
    return;
  }

  if (auto *s = dynamic_cast<PrintStmt *>(stmt)) {

    Value *v = lowerExpr(cg, s->e.get());
    Type *ty = v->getType();

    if (ty->isIntegerTy(32)) {
      cg.emitPrintfInt(v);
    } else if (ty->isDoubleTy() || ty->isFloatTy()) {
      cg.emitPrintfFloat(v);
    } else if (ty->isIntegerTy(1)) {
      cg.emitPrintfBool(v);
    } else {
      llvm_unreachable("Unsupported print type");
    }

    return;
  }

  if (auto *s = dynamic_cast<BlockStmt *>(stmt)) {
    lowerBlock(cg, s);
    return;
  }

  if (auto *s = dynamic_cast<IfStmt *>(stmt)) {
    lowerIfStmt(cg, s);
    return;
  }

  if (auto *s = dynamic_cast<WhileStmt *>(stmt)) {
    lowerWhileStmt(cg, s);
    return;
  }

  if (auto *s = dynamic_cast<ForStmt *>(stmt)) {
    lowerForStmt(cg, s);
    return;
  }

  if (auto *s = dynamic_cast<ReturnStmt *>(stmt)) {
    lowerReturnStmt(cg, s);
    return;
  }

  if (auto *s = dynamic_cast<FunctionStmt *>(stmt)) {
    lowerFunctionStmt(cg, s);
    return;
  }

  if (auto *s = dynamic_cast<VarDeclStmt *>(stmt)) {
    lowerVarDeclStmt(cg, s);
    return;
  }

  llvm_unreachable("unhandled stmt");
}
