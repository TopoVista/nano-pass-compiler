#include "codegen/llvm_codegen.h"

using namespace llvm;

/* ================= TYPE LOWERING ================= */

Type *LLVMCodegen::toLLVMType(const LangType &t) {

  switch (t.kind) {

  case LangTypeKind::Integer:
    return IntegerType::get(ctx, t.bitWidth);

  case LangTypeKind::Floating:
    if (t.bitWidth == 32)
      return Type::getFloatTy(ctx);
    if (t.bitWidth == 64)
      return Type::getDoubleTy(ctx);
    break;

  case LangTypeKind::Bool:
    return Type::getInt1Ty(ctx);

  case LangTypeKind::Char:
    return Type::getInt8Ty(ctx);

  case LangTypeKind::Array: {

    if (!t.element)
      llvm_unreachable("Array missing element type");

    if (t.arraySize <= 0)
      llvm_unreachable("Array must have positive size");

    Type *elemType = toLLVMType(*t.element);

    return ArrayType::get(elemType, t.arraySize);
  }

  case LangTypeKind::Void:
    return Type::getVoidTy(ctx);

  default:
    break;
  }

  llvm_unreachable("Unsupported type in LLVM lowering");
}

/* ================= PRINTF SUPPORT ================= */

Function *LLVMCodegen::getPrintf() {

  Function *printfFn = module->getFunction("printf");
  if (printfFn)
    return printfFn;

  auto *i8ptr = PointerType::getUnqual(Type::getInt8Ty(ctx));
  auto *fnTy = FunctionType::get(Type::getInt32Ty(ctx), {i8ptr}, true);

  printfFn =
      Function::Create(fnTy, Function::ExternalLinkage, "printf", module);

  return printfFn;
}

/* ================= PRINT INT ================= */

void LLVMCodegen::emitPrintfInt(Value *v) {

  if (!v->getType()->isIntegerTy(32))
    v = builder.CreateSExtOrTrunc(v, Type::getInt32Ty(ctx));

  Value *fmt = builder.CreateGlobalStringPtr("%d\n");
  builder.CreateCall(getPrintf(), {fmt, v});
}

/* ================= PRINT FLOAT ================= */

void LLVMCodegen::emitPrintfFloat(Value *val) {

  if (val->getType()->isFloatTy())
    val = builder.CreateFPExt(val, Type::getDoubleTy(ctx));

  Value *formatStr = builder.CreateGlobalStringPtr("%f\n");
  builder.CreateCall(getPrintf(), {formatStr, val});
}

/* ================= PRINT BOOL ================= */

void LLVMCodegen::emitPrintfBool(Value *val) {

  if (!val->getType()->isIntegerTy(1))
    val = builder.CreateICmpNE(val, ConstantInt::get(val->getType(), 0));

  val = builder.CreateZExt(val, Type::getInt32Ty(ctx));

  emitPrintfInt(val);
}

/* ================= PRINT STRING ================= */

void LLVMCodegen::emitPrintfStr(Value *v) {

  Value *fmt = builder.CreateGlobalStringPtr("%s\n");
  builder.CreateCall(getPrintf(), {fmt, v});
}
