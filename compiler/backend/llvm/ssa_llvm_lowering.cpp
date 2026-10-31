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
    for (const auto &[local, type] : source.memoryLocals)
      ctx.locals.emplace(local, entryBuilder.CreateAlloca(lowerType(context_, type), nullptr, local.str()));

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
        const auto type = lowerType(context_, instruction.type);
        auto op1 = [&] { return ctx.operand(instruction.operand1, type, block.id); };
        auto op2 = [&](llvm::Type *expected) { return ctx.operand(instruction.operand2, expected, block.id); };
        switch (instruction.kind) {
        case SSAInstructionKind::Constant: result = constant(ctx.builder, instruction.constantValue, type); break;
        case SSAInstructionKind::Copy: result = op1(); break;
        case SSAInstructionKind::Unary: {
          auto *operand = op1();
          result = instruction.unaryOp == UnaryOpKind::Negate
              ? (operand->getType()->isFloatingPointTy() ? ctx.builder.CreateFNeg(operand) : ctx.builder.CreateNeg(operand))
              : ctx.builder.CreateNot(operand); break;
        }
        case SSAInstructionKind::Binary: {
          llvm::Type *operandType = type;
          if (std::holds_alternative<ValueId>(instruction.operand1)) operandType = op1()->getType();
          auto *left = ctx.operand(instruction.operand1, operandType, block.id);
          auto *right = op2(operandType); const bool fp = left->getType()->isFloatingPointTy();
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
            llvm::Type *guess = type;
            if (std::holds_alternative<ValueId>(instruction.callArgs.front())) {
              guess = ctx.operand(instruction.callArgs.front(), type, block.id)->getType();
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
        case SSAInstructionKind::LoadLocal: { auto it=ctx.locals.find(instruction.local); if(it==ctx.locals.end())ctx.fail("unknown memory local",block.id,instruction.result); result=ctx.builder.CreateLoad(type,it->second); break; }
        case SSAInstructionKind::StoreLocal: { auto it=ctx.locals.find(instruction.local); if(it==ctx.locals.end())ctx.fail("unknown memory local",block.id); ctx.builder.CreateStore(op1(),it->second); break; }
        case SSAInstructionKind::LoadIndex: {
          const auto local = source.memoryLocals.find(instruction.local);
          if (local == source.memoryLocals.end() || local->second.kind != LangTypeKind::Array || !local->second.element)
            ctx.fail("indexed load references a non-array memory local", block.id, instruction.result);
          const auto slot = ctx.locals.find(instruction.local);
          if (slot == ctx.locals.end()) ctx.fail("unknown array memory local", block.id, instruction.result);
          auto *arrayType = llvm::cast<llvm::ArrayType>(lowerType(context_, local->second));
          auto *index = ctx.operand(instruction.operand1, llvm::Type::getInt64Ty(context_), block.id);
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

} // namespace nanopass::llvm_backend
