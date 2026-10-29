#pragma once

#include <map>
#include <stdexcept>

#include "interpreter/execution.h"

namespace nanopass {

class CoreInterpreter {
  enum class Flow { Normal, Break, Continue, Return };
  struct Frame { std::map<LocalId, RuntimeValue> locals; };
  const CoreProgram *program_ = nullptr;
  ExecutionLimits limits_;
  ExecutionResult result_;
  size_t callDepth_ = 0;

  [[noreturn]] void fail(ExecutionStatus status, std::string category, std::string message) {
    result_.status = status;
    result_.error = RuntimeError{std::move(category), std::move(message)};
    throw std::runtime_error("runtime stop");
  }
  void step() { if (++result_.stepsExecuted > limits_.maxSteps) fail(ExecutionStatus::StepLimitExceeded, "step-limit", "execution step limit exceeded"); }
  bool truth(const RuntimeValue &v) {
    if (auto b=std::get_if<bool>(&v)) return *b;
    if (auto i=std::get_if<int64_t>(&v)) return *i != 0;
    fail(ExecutionStatus::RuntimeError, "type-error", "condition is not boolean/integer");
  }
  RuntimeValue constant(const CoreConstantVal &v) { return std::visit([](const auto &x)->RuntimeValue{return x;},v); }
  RuntimeValue lookup(const Frame &frame, const LocalId &id) {
    auto it=frame.locals.find(id); if(it==frame.locals.end()) fail(ExecutionStatus::RuntimeError,"uninitialized-local","read of "+id.str()); return it->second;
  }
  RuntimeValue unary(UnaryOpKind op, const RuntimeValue &v) {
    if(op==UnaryOpKind::LogicalNot) return !truth(v);
    if(auto i=std::get_if<int64_t>(&v)) return -*i;
    if(auto d=std::get_if<double>(&v)) return -*d;
    fail(ExecutionStatus::RuntimeError,"type-error","invalid unary operand");
  }
  RuntimeValue binary(BinaryOpKind op, const RuntimeValue &a, const RuntimeValue &b) {
    if(op==BinaryOpKind::LogicalAnd) return truth(a) && truth(b);
    if(op==BinaryOpKind::LogicalOr) return truth(a) || truth(b);
    if(auto x=std::get_if<int64_t>(&a)) if(auto y=std::get_if<int64_t>(&b)) {
      switch(op){case BinaryOpKind::Add:return *x+*y;case BinaryOpKind::Sub:return *x-*y;case BinaryOpKind::Mul:return *x**y;case BinaryOpKind::Div:if(!*y)fail(ExecutionStatus::RuntimeError,"division-by-zero","integer division by zero");return *x / *y;case BinaryOpKind::Mod:if(!*y)fail(ExecutionStatus::RuntimeError,"division-by-zero","integer remainder by zero");return *x % *y;case BinaryOpKind::Eq:return *x==*y;case BinaryOpKind::Ne:return *x!=*y;case BinaryOpKind::Lt:return *x<*y;case BinaryOpKind::Le:return *x<=*y;case BinaryOpKind::Gt:return *x>*y;case BinaryOpKind::Ge:return *x>=*y;default:break;}}
    if(auto x=std::get_if<double>(&a)) if(auto y=std::get_if<double>(&b)) { switch(op){case BinaryOpKind::Add:return *x+*y;case BinaryOpKind::Sub:return *x-*y;case BinaryOpKind::Mul:return *x**y;case BinaryOpKind::Div:return *x / *y;case BinaryOpKind::Eq:return *x==*y;case BinaryOpKind::Ne:return *x!=*y;case BinaryOpKind::Lt:return *x<*y;case BinaryOpKind::Le:return *x<=*y;case BinaryOpKind::Gt:return *x>*y;case BinaryOpKind::Ge:return *x>=*y;default:break;}}
    if(op==BinaryOpKind::Eq) return runtimeValueText(a)==runtimeValueText(b); if(op==BinaryOpKind::Ne) return runtimeValueText(a)!=runtimeValueText(b);
    fail(ExecutionStatus::RuntimeError,"type-error","invalid binary operands");
  }
  RuntimeValue eval(const CoreExpr &expr, Frame &frame) {
    step();
    if(auto e=dynamic_cast<const CoreConstantExpr*>(&expr)) return constant(e->value);
    if(auto e=dynamic_cast<const CoreVariableRefExpr*>(&expr)) return lookup(frame,e->id);
    if(auto e=dynamic_cast<const CoreUnaryExpr*>(&expr)) return unary(e->op,eval(*e->operand,frame));
    if(auto e=dynamic_cast<const CoreBinaryExpr*>(&expr)) { auto left=eval(*e->left,frame); if(e->op==BinaryOpKind::LogicalAnd&&!truth(left))return false; if(e->op==BinaryOpKind::LogicalOr&&truth(left))return true; return binary(e->op,left,eval(*e->right,frame)); }
    if(auto e=dynamic_cast<const CoreCallExpr*>(&expr)) { std::vector<RuntimeValue> args;for(const auto&a:e->args)args.push_back(eval(*a,frame));return call(e->callee,args); }
    fail(ExecutionStatus::UnsupportedFeature,"core-index","Core array execution is not implemented");
  }
  Flow statements(const std::vector<std::unique_ptr<CoreStmt>> &stmts, Frame &frame, std::optional<RuntimeValue> &returned) {
    for(const auto &stmt:stmts) { step(); if(auto s=dynamic_cast<const CoreVarDeclStmt*>(stmt.get())) { if(s->initializer)frame.locals[s->id]=eval(*s->initializer,frame); else fail(ExecutionStatus::RuntimeError,"uninitialized-local","uninitialized declaration "+s->id.str()); }
      else if(auto s=dynamic_cast<const CoreAssignStmt*>(stmt.get())) frame.locals[s->target]=eval(*s->value,frame);
      else if(auto s=dynamic_cast<const CoreExprStmt*>(stmt.get())) (void)eval(*s->expr,frame);
      else if(auto s=dynamic_cast<const CoreIfStmt*>(stmt.get())) {auto f=statements(truth(eval(*s->condition,frame))?s->thenBranch:s->elseBranch,frame,returned);if(f!=Flow::Normal)return f;}
      else if(auto s=dynamic_cast<const CoreWhileStmt*>(stmt.get())) {while(truth(eval(*s->condition,frame))){auto f=statements(s->body,frame,returned);if(f==Flow::Return)return f;if(f==Flow::Break)break;}}
      else if(auto s=dynamic_cast<const CoreReturnStmt*>(stmt.get())) {if(s->value)returned=eval(*s->value,frame);return Flow::Return;}
      else if(dynamic_cast<const CoreBreakStmt*>(stmt.get()))return Flow::Break; else if(dynamic_cast<const CoreContinueStmt*>(stmt.get()))return Flow::Continue;
      else fail(ExecutionStatus::UnsupportedFeature,"core-statement","unsupported Core statement"); }
    return Flow::Normal;
  }
  RuntimeValue call(const std::string &name, const std::vector<RuntimeValue> &args) {
    if(name=="print"){if(args.size()!=1)fail(ExecutionStatus::RuntimeError,"arity","print expects one argument");auto text=runtimeValueText(args[0])+"\n";if(result_.stdoutText.size()+text.size()>limits_.maxOutputBytes)fail(ExecutionStatus::OutputLimitExceeded,"output-limit","output limit exceeded");result_.stdoutText+=text;return int64_t(0);}
    const CoreFunction *fn=nullptr;for(const auto &candidate:program_->functions)if(candidate.name==name){fn=&candidate;break;}if(!fn)fail(ExecutionStatus::RuntimeError,"unknown-function","unknown function @"+name);if(args.size()!=fn->params.size())fail(ExecutionStatus::RuntimeError,"arity","wrong argument count");if(++callDepth_>limits_.maxCallDepth)fail(ExecutionStatus::RecursionLimitExceeded,"recursion-limit","call depth limit exceeded");Frame frame;for(size_t i=0;i<args.size();++i)frame.locals[fn->params[i].id]=args[i];std::optional<RuntimeValue> returned;auto flow=statements(fn->body,frame,returned);--callDepth_;if(flow!=Flow::Return&&fn->returnType.kind!=LangTypeKind::Void)fail(ExecutionStatus::RuntimeError,"missing-return","non-void function returned no value");return returned.value_or(RuntimeValue{int64_t(0)});
  }
public:
  ExecutionResult run(const CoreProgram &program, std::string entry="main", ExecutionLimits limits={}) { program_=&program;limits_=limits;result_={};callDepth_=0;try { std::vector<RuntimeValue> none;auto value=call(entry,none);const CoreFunction*fn=nullptr;for(const auto&f:program.functions)if(f.name==entry)fn=&f;if(fn&&fn->returnType.kind!=LangTypeKind::Void)result_.returnValue=value; } catch(const std::runtime_error&) { if(result_.status==ExecutionStatus::Success) {result_.status=ExecutionStatus::InternalError;result_.error=RuntimeError{"internal","interpreter failure"};} } return result_; }
};

} // namespace nanopass
