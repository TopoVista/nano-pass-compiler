#include "backend/llvm/ssa_llvm_lowering.h"

#include <map>
#include <sstream>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "ir/ssa/ssa_verifier.h"

namespace nanopass::llvm_backend {
namespace {

std::string diagnosticText(const LoweringDiagnostic &d) {
  std::ostringstream out;
  out << "SSA-to-LLVM lowering failed";
  if (!d.function.empty()) out << " in @" << d.function;
  if (d.block) out << " " << d.block->str();
  if (d.value) out << " " << d.value->str();
  out << ": " << d.message;
  return out.str();
}

llvm::Type *lowerType(llvm::LLVMContext &context, const LangType &type) {
  switch (type.kind) {
  case LangTypeKind::Integer: return llvm::IntegerType::get(context, type.bitWidth);
  case LangTypeKind::Floating:
    if (type.bitWidth == 32) return llvm::Type::getFloatTy(context);
    if (type.bitWidth == 64) return llvm::Type::getDoubleTy(context);
    break;
  case LangTypeKind::Bool: return llvm::Type::getInt1Ty(context);
  case LangTypeKind::Char: return llvm::Type::getInt8Ty(context);
  case LangTypeKind::String: return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
  case LangTypeKind::Array:
    if (type.element && type.arraySize >= 0)
      return llvm::ArrayType::get(lowerType(context, *type.element), type.arraySize);
    break;
  case LangTypeKind::Void: return llvm::Type::getVoidTy(context);
  default: break;
  }
  throw LoweringError({"unsupported Nano type", {}, {}, {}});
}

llvm::Value *constant(llvm::IRBuilder<> &builder, const CoreConstantVal &value,
                      llvm::Type *expected) {
  return std::visit([&](const auto &item) -> llvm::Value * {
    using T = std::decay_t<decltype(item)>;
    if constexpr (std::is_same_v<T, int64_t>) {
      if (!expected->isIntegerTy()) throw std::runtime_error("integer constant has non-integer expected type");
      return llvm::ConstantInt::get(expected, static_cast<uint64_t>(item), true);
    } else if constexpr (std::is_same_v<T, double>) {
      if (!expected->isFloatingPointTy()) throw std::runtime_error("float constant has non-float expected type");
      return llvm::ConstantFP::get(expected, item);
    } else if constexpr (std::is_same_v<T, bool>) {
      if (!expected->isIntegerTy(1)) throw std::runtime_error("bool constant has non-bool expected type");
      return llvm::ConstantInt::get(expected, item);
    } else {
      if (!expected->isPointerTy()) throw std::runtime_error("string constant has non-string expected type");
      return builder.CreateGlobalStringPtr(item);
    }
  }, value);
}

llvm::Type *defaultConstantType(llvm::LLVMContext &context,
                                const CoreConstantVal &value) {
  return std::visit([&](const auto &item) -> llvm::Type * {
    using T = std::decay_t<decltype(item)>;
    // Untagged integer constants are the source language's ordinary `int`.
    // Values with a narrower/wider declared type are recovered from a mapped
    // ValueId before this fallback is used.
    if constexpr (std::is_same_v<T, int64_t>) return llvm::Type::getInt32Ty(context);
    else if constexpr (std::is_same_v<T, double>) return llvm::Type::getDoubleTy(context);
    else if constexpr (std::is_same_v<T, bool>) return llvm::Type::getInt1Ty(context);
    else return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
  }, value);
}

llvm::Function *runtimePrint(llvm::Module &module, llvm::Type *argument) {
  llvm::LLVMContext &context = module.getContext();
  std::string name;
  if (argument->isIntegerTy(1)) name = "nano_print_bool";
  else if (argument->isIntegerTy(8)) name = "nano_print_i8";
  else if (argument->isIntegerTy(16)) name = "nano_print_i16";
  else if (argument->isIntegerTy(32)) name = "nano_print_i32";
  else if (argument->isIntegerTy(64)) name = "nano_print_i64";
  else if (argument->isFloatTy()) name = "nano_print_f32";
  else if (argument->isDoubleTy()) name = "nano_print_f64";
  else if (argument->isPointerTy()) name = "nano_print_string";
  else throw std::runtime_error("print argument has unsupported type");
  if (auto *existing = module.getFunction(name)) return existing;
  return llvm::Function::Create(llvm::FunctionType::get(
      llvm::Type::getVoidTy(context), {argument}, false),
      llvm::Function::ExternalLinkage, name, module);
}

llvm::Function *runtimeArrayBoundsCheck(llvm::Module &module) {
  if (auto *existing = module.getFunction("nano_check_array_bounds_i64")) return existing;
  auto &context = module.getContext();
  auto *i64 = llvm::Type::getInt64Ty(context);
  return llvm::Function::Create(llvm::FunctionType::get(
      llvm::Type::getVoidTy(context), {i64, i64}, false),
      llvm::Function::ExternalLinkage, "nano_check_array_bounds_i64", module);
}

llvm::Value *asSignedI64(llvm::IRBuilder<> &builder, llvm::Value *value) {
  auto *i64 = llvm::Type::getInt64Ty(builder.getContext());
  if (!value->getType()->isIntegerTy())
    throw std::runtime_error("array index is not an integer");
  const unsigned width = llvm::cast<llvm::IntegerType>(value->getType())->getBitWidth();
  if (width == 64) return value;
  if (width < 64) return builder.CreateSExt(value, i64, "array.index.i64");
  return builder.CreateTrunc(value, i64, "array.index.i64");
}

struct FunctionContext {
  llvm::Module &module;
  llvm::IRBuilder<> builder;
  const SSAFunction &source;
  llvm::Function &function;
  std::map<int, llvm::BasicBlock *> blocks;
  std::map<int, llvm::Value *> values;
  std::map<LocalId, llvm::Value *> locals;

  FunctionContext(llvm::Module &m, const SSAFunction &s, llvm::Function &f)
      : module(m), builder(m.getContext()), source(s), function(f) {}

  [[noreturn]] void fail(std::string message, std::optional<BlockId> block = {},
                         std::optional<ValueId> value = {}) const {
    throw LoweringError({std::move(message), source.name, block, value});
  }

  llvm::Value *operand(const SSAOperand &item, llvm::Type *expected,
                       BlockId block) {
    if (const auto *value = std::get_if<ValueId>(&item)) {
      const auto it = values.find(value->index);
      if (it == values.end()) fail("use has no LLVM value mapping", block, *value);
      return it->second;
    }
    try { return constant(builder, std::get<CoreConstantVal>(item), expected); }
    catch (const std::exception &error) { fail(error.what(), block); }
  }
};

} // namespace

LoweringError::LoweringError(LoweringDiagnostic diagnostic)
    : std::runtime_error(diagnosticText(diagnostic)), diagnostic_(std::move(diagnostic)) {}

LoweringResult SSALLVMLowering::lower(const SSAProgram &program,
                                      std::string moduleName) const {
  auto module = std::make_unique<llvm::Module>(std::move(moduleName), context_);
  std::map<std::string, llvm::Function *> functions;
  for (const auto &source : program.functions) {
    const auto check = verifySSA(source);
    if (!check.ok) throw LoweringError({check.errors.front(), source.name, {}, {}});
    std::vector<llvm::Type *> parameters;
    for (const auto &parameter : source.parameters) parameters.push_back(lowerType(context_, parameter.type));
    auto *type = llvm::FunctionType::get(lowerType(context_, source.returnType), parameters, false);
    if (functions.contains(source.name)) throw LoweringError({"duplicate function name", source.name, {}, {}});
    functions.emplace(source.name, llvm::Function::Create(type, llvm::Function::ExternalLinkage, source.name, module.get()));
  }

  LoweringStatistics statistics;
  for (const auto &source : program.functions) {
    llvm::Function &function = *functions.at(source.name);
    FunctionContext ctx(*module, source, function);
    size_t arg = 0;
    for (auto &argument : function.args()) ctx.values.emplace(source.parameters[arg++].value.index, &argument);
    for (const auto &block : source.blocks)
      ctx.blocks.emplace(block.id.index, llvm::BasicBlock::Create(context_, "bb" + std::to_string(block.id.index), &function));
    auto *entry = ctx.blocks.at(source.entryBlock.index);
    llvm::IRBuilder<> entryBuilder(entry, entry->begin());
    for (const auto &[local, type] : source.memoryLocals) {
      auto *storageType = lowerType(context_, type);
      auto *slot = entryBuilder.CreateAlloca(storageType, nullptr, local.str());
      // The reference engines define freshly allocated fixed arrays as zeroed.
      // Scalar locals intentionally retain their existing uninitialized model.
      if (type.kind == LangTypeKind::Array)
        entryBuilder.CreateStore(llvm::ConstantAggregateZero::get(storageType), slot);
      ctx.locals.emplace(local, slot);
    }

    // Pre-create phis before instructions, then fill incoming edges after all
    // definitions have mappings. This is essential for loop and inline phis.
    for (const auto &block : source.blocks) {
      ctx.builder.SetInsertPoint(ctx.blocks.at(block.id.index));
      for (const auto &phi : block.phis) {
        auto *node = ctx.builder.CreatePHI(lowerType(context_, phi.type), phi.incoming.size(), phi.result.str());
        ctx.values.emplace(phi.result.index, node);
        ++statistics.phis;
      }
    }
    for (const auto &block : source.blocks) {
      ctx.builder.SetInsertPoint(ctx.blocks.at(block.id.index));
      for (const auto &instruction : block.instructions) {
        llvm::Value *result = nullptr;
        auto declaredResultType = [&]() -> llvm::Type * {
          try { return lowerType(context_, instruction.type); }
          catch (const LoweringError &) { ctx.fail("unsupported result type", block.id, instruction.result); }
        };
        auto op1 = [&](llvm::Type *expected) { return ctx.operand(instruction.operand1, expected, block.id); };
        auto op2 = [&](llvm::Type *expected) { return ctx.operand(instruction.operand2, expected, block.id); };
        auto operandType = [&](const SSAOperand &operand) -> llvm::Type * {
          if (std::holds_alternative<ValueId>(operand))
            return ctx.operand(operand, llvm::Type::getInt32Ty(context_), block.id)->getType();
          return defaultConstantType(context_, std::get<CoreConstantVal>(operand));
        };
        switch (instruction.kind) {
        case SSAInstructionKind::Constant: {
          llvm::Type *type = instruction.type.kind == LangTypeKind::Unknown
              ? defaultConstantType(context_, instruction.constantValue) : declaredResultType();
          result = constant(ctx.builder, instruction.constantValue, type); break;
        }
        case SSAInstructionKind::Copy: result = op1(operandType(instruction.operand1)); break;
        case SSAInstructionKind::Unary: {
          auto *operand = op1(operandType(instruction.operand1));
          result = instruction.unaryOp == UnaryOpKind::Negate
              ? (operand->getType()->isFloatingPointTy() ? ctx.builder.CreateFNeg(operand) : ctx.builder.CreateNeg(operand))
              : ctx.builder.CreateNot(operand); break;
        }
        case SSAInstructionKind::Binary: {
          llvm::Type *type = operandType(instruction.operand1);
          auto *left = ctx.operand(instruction.operand1, type, block.id);
          auto *right = op2(type); const bool fp = left->getType()->isFloatingPointTy();
          switch (instruction.binaryOp) {
          case BinaryOpKind::Add: result=fp?ctx.builder.CreateFAdd(left,right):ctx.builder.CreateAdd(left,right); break;
          case BinaryOpKind::Sub: result=fp?ctx.builder.CreateFSub(left,right):ctx.builder.CreateSub(left,right); break;
          case BinaryOpKind::Mul: result=fp?ctx.builder.CreateFMul(left,right):ctx.builder.CreateMul(left,right); break;
          case BinaryOpKind::Div: result=fp?ctx.builder.CreateFDiv(left,right):ctx.builder.CreateSDiv(left,right); break;
          case BinaryOpKind::Mod: result=fp?ctx.builder.CreateFRem(left,right):ctx.builder.CreateSRem(left,right); break;
          case BinaryOpKind::Eq: result=fp?ctx.builder.CreateFCmpOEQ(left,right):ctx.builder.CreateICmpEQ(left,right); break;
          case BinaryOpKind::Ne: result=fp?ctx.builder.CreateFCmpONE(left,right):ctx.builder.CreateICmpNE(left,right); break;
          case BinaryOpKind::Lt: result=fp?ctx.builder.CreateFCmpOLT(left,right):ctx.builder.CreateICmpSLT(left,right); break;
          case BinaryOpKind::Le: result=fp?ctx.builder.CreateFCmpOLE(left,right):ctx.builder.CreateICmpSLE(left,right); break;
          case BinaryOpKind::Gt: result=fp?ctx.builder.CreateFCmpOGT(left,right):ctx.builder.CreateICmpSGT(left,right); break;
          case BinaryOpKind::Ge: result=fp?ctx.builder.CreateFCmpOGE(left,right):ctx.builder.CreateICmpSGE(left,right); break;
          case BinaryOpKind::LogicalAnd: result=ctx.builder.CreateAnd(left,right); break;
          case BinaryOpKind::LogicalOr: result=ctx.builder.CreateOr(left,right); break;
          } break;
        }
        case SSAInstructionKind::Call: {
          std::vector<llvm::Value *> arguments;
          if (instruction.callee == "print") {
            if (instruction.callArgs.size() != 1) ctx.fail("print expects one argument", block.id, instruction.result);
            llvm::Type *guess = instruction.type.kind == LangTypeKind::Void
                ? llvm::Type::getInt64Ty(context_) : declaredResultType();
            if (std::holds_alternative<ValueId>(instruction.callArgs.front())) {
              guess = ctx.operand(instruction.callArgs.front(), guess, block.id)->getType();
            } else {
              const auto &literal = std::get<CoreConstantVal>(instruction.callArgs.front());
              guess = std::visit([&](const auto &item) -> llvm::Type * {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, int64_t>) return llvm::Type::getInt64Ty(context_);
                else if constexpr (std::is_same_v<T, double>) return llvm::Type::getDoubleTy(context_);
                else if constexpr (std::is_same_v<T, bool>) return llvm::Type::getInt1Ty(context_);
                else return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context_));
              }, literal);
            }
            auto *value = ctx.operand(instruction.callArgs.front(), guess, block.id);
            ctx.builder.CreateCall(runtimePrint(*module, value->getType()), {value});
            break;
          }
          const auto found = functions.find(instruction.callee);
          if (found == functions.end()) ctx.fail("unknown direct callee @" + instruction.callee, block.id, instruction.result);
          auto *callee = found->second;
          if (callee->arg_size() != instruction.callArgs.size()) ctx.fail("call arity mismatch", block.id, instruction.result);
          size_t index=0; for (const auto &argument : instruction.callArgs) arguments.push_back(ctx.operand(argument, callee->getFunctionType()->getParamType(index++), block.id));
          auto *call = ctx.builder.CreateCall(callee, arguments);
          if (!callee->getReturnType()->isVoidTy()) result = call;
          break;
        }
        case SSAInstructionKind::LoadLocal: {
          auto it=ctx.locals.find(instruction.local); if(it==ctx.locals.end())ctx.fail("unknown memory local",block.id,instruction.result);
          const auto sourceLocal=source.memoryLocals.find(instruction.local);
          if(sourceLocal==source.memoryLocals.end())ctx.fail("load references a promoted or unknown local",block.id,instruction.result);
          result=ctx.builder.CreateLoad(lowerType(context_,sourceLocal->second),it->second); break;
        }
        case SSAInstructionKind::StoreLocal: {
          auto it=ctx.locals.find(instruction.local); if(it==ctx.locals.end())ctx.fail("unknown memory local",block.id);
          const auto sourceLocal=source.memoryLocals.find(instruction.local);
          if(sourceLocal==source.memoryLocals.end())ctx.fail("store references a promoted or unknown local",block.id);
          ctx.builder.CreateStore(op1(lowerType(context_,sourceLocal->second)),it->second); break;
        }
        case SSAInstructionKind::LoadIndex: {
          const auto local = source.memoryLocals.find(instruction.local);
          if (local == source.memoryLocals.end() || local->second.kind != LangTypeKind::Array || !local->second.element)
            ctx.fail("indexed load references a non-array memory local", block.id, instruction.result);
          const auto slot = ctx.locals.find(instruction.local);
          if (slot == ctx.locals.end()) ctx.fail("unknown array memory local", block.id, instruction.result);
          auto *arrayType = llvm::cast<llvm::ArrayType>(lowerType(context_, local->second));
          auto *index = ctx.operand(instruction.operand1, llvm::Type::getInt64Ty(context_), block.id);
          try {
            ctx.builder.CreateCall(runtimeArrayBoundsCheck(*module), {
                asSignedI64(ctx.builder, index),
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), local->second.arraySize, true)});
          } catch (const std::exception &error) { ctx.fail(error.what(), block.id, instruction.result); }
          auto *zero = llvm::ConstantInt::get(index->getType(), 0);
          auto *address = ctx.builder.CreateGEP(arrayType, slot->second, {zero, index});
          result = ctx.builder.CreateLoad(lowerType(context_, *local->second.element), address);
          break;
        }
        case SSAInstructionKind::StoreIndex: {
          const auto local = source.memoryLocals.find(instruction.local);
          if (local == source.memoryLocals.end() || local->second.kind != LangTypeKind::Array || !local->second.element)
            ctx.fail("indexed store references a non-array memory local", block.id);
          const auto slot = ctx.locals.find(instruction.local);
          if (slot == ctx.locals.end()) ctx.fail("unknown array memory local", block.id);
          auto *arrayType = llvm::cast<llvm::ArrayType>(lowerType(context_, local->second));
          auto *index = ctx.operand(instruction.operand1, llvm::Type::getInt64Ty(context_), block.id);
          try {
            ctx.builder.CreateCall(runtimeArrayBoundsCheck(*module), {
                asSignedI64(ctx.builder, index),
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), local->second.arraySize, true)});
          } catch (const std::exception &error) { ctx.fail(error.what(), block.id); }
          auto *zero = llvm::ConstantInt::get(index->getType(), 0);
          auto *address = ctx.builder.CreateGEP(arrayType, slot->second, {zero, index});
          ctx.builder.CreateStore(ctx.operand(instruction.operand2, lowerType(context_, *local->second.element), block.id), address);
          break;
        }
        }
        if (instruction.result) { if (!result) ctx.fail("value-producing instruction has no result", block.id, instruction.result); ctx.values.emplace(instruction.result->index, result); }
        ++statistics.instructions;
      }
      std::visit([&](const auto &term) {
        using T=std::decay_t<decltype(term)>;
        if constexpr(std::is_same_v<T,SSATerminator::Jump>) ctx.builder.CreateBr(ctx.blocks.at(term.target.index));
        else if constexpr(std::is_same_v<T,SSATerminator::Branch>) { auto *condition=ctx.operand(term.condition,llvm::Type::getInt1Ty(context_),block.id); if(!condition->getType()->isIntegerTy(1))ctx.fail("branch condition is not i1",block.id); ctx.builder.CreateCondBr(condition,ctx.blocks.at(term.thenTarget.index),ctx.blocks.at(term.elseTarget.index)); }
        else if constexpr(std::is_same_v<T,SSATerminator::Return>) { if(term.value)ctx.builder.CreateRet(ctx.operand(*term.value,function.getReturnType(),block.id)); else ctx.builder.CreateRetVoid(); }
        else ctx.builder.CreateUnreachable();
      }, block.terminator.value);
      ++statistics.blocks;
    }
    for (const auto &block : source.blocks) for (const auto &phi : block.phis) {
      auto *node=llvm::cast<llvm::PHINode>(ctx.values.at(phi.result.index));
      for(const auto &incoming:phi.incoming) node->addIncoming(ctx.values.at(incoming.value.index),ctx.blocks.at(incoming.predecessor.index));
    }
    std::string error; llvm::raw_string_ostream stream(error);
    if (llvm::verifyFunction(function,&stream)) throw LoweringError({stream.str(),source.name,{},{}});
    ++statistics.functions;
  }
  std::string error; llvm::raw_string_ostream stream(error);
  if (llvm::verifyModule(*module,&stream)) throw LoweringError({stream.str(),{}, {}, {}});
  return {std::move(module), statistics};
}

LoweringResult SSALLVMLowering::lowerFunction(const SSAProgram &program,
                                               std::string_view functionName,
                                               std::string implementationName,
                                               std::string moduleName) const {
  // Reuse the established whole-program lowering path to preserve every
  // opcode, type rule, and verifier gate. The non-selected definitions are
  // then converted to declarations: the ORC adaptive runtime resolves those
  // public names through stable dispatch stubs.
  auto lowered = lower(program, std::move(moduleName));
  llvm::Function *body = lowered.module->getFunction(std::string(functionName));
  if (!body)
    throw LoweringError({"unknown function selected for function-level lowering",
                         std::string(functionName), {}, {}});
  if (lowered.module->getFunction(implementationName))
    throw LoweringError({"implementation symbol already exists", std::string(functionName), {}, {}});
  body->setName(implementationName);
  for (const auto &source : program.functions) {
    if (source.name == functionName) continue;
    if (llvm::Function *declaration = lowered.module->getFunction(source.name))
      declaration->deleteBody();
  }
  std::string verifierText;
  llvm::raw_string_ostream verifierStream(verifierText);
  if (llvm::verifyModule(*lowered.module, &verifierStream))
    throw LoweringError({"function-level LLVM verification failed: " + verifierStream.str(),
                         std::string(functionName), {}, {}});
  lowered.statistics.functions = 1;
  return lowered;
}

} // namespace nanopass::llvm_backend
