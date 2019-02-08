/*----------------------------------------------------------------------------*\
|*
|* Scope analysis AST walker for tracking lifetimes
|*
L*----------------------------------------------------------------------------*/
#include "bonScopeAnalysisPass.h"

namespace bon {

ScopeAnalysisPass::ScopeAnalysisPass(ModuleState &state)
  : state_(state)
{
}

// NumberExprAST
void ScopeAnalysisPass::process(NumberExprAST* node) {
  node->ends_scope_ = true;
}

// IntegerExprAST
void ScopeAnalysisPass::process(IntegerExprAST* node) {
  node->ends_scope_ = true;
}

// StringExprAST
void ScopeAnalysisPass::process(StringExprAST* node) {
  node->ends_scope_ = true;
}

// BoolExprAST
void ScopeAnalysisPass::process(BoolExprAST* node) {
  node->ends_scope_ = true;
}

// UnitExprAST
void ScopeAnalysisPass::process(UnitExprAST* node) {
  node->ends_scope_ = true;
}

// VariableExprAST
void ScopeAnalysisPass::process(VariableExprAST* node) {
  node->ends_scope_ = true;
}

// ValueConstructorExprAST
void ScopeAnalysisPass::process(ValueConstructorExprAST* node) {
  node->ends_scope_ = true;
}

// UnaryExprAST
void ScopeAnalysisPass::process(UnaryExprAST* node) {
  node->ends_scope_ = true;
}

// BinaryExprAST
void ScopeAnalysisPass::process(BinaryExprAST* node) {
  auto binop = dynamic_cast<BinaryExprAST*>(node->RHS.get());
  if (node->Op == tok_sep || binop) {
    node->RHS->run_pass(this);
  }
  else {
    node->ends_scope_ = true;
  }
}

// IfExprAST
void ScopeAnalysisPass::process(IfExprAST* node) {
  node->Then->run_pass(this);
  if (node->Else) {
    node->Else->run_pass(this);
  }
}

// WhileExprAST
void ScopeAnalysisPass::process(WhileExprAST* node) {
  node->ends_scope_ = true;
}

// MatchCaseExprAST
void ScopeAnalysisPass::process(MatchCaseExprAST* node) {
  node->body_->run_pass(this);
}

// MatchExprAST
void ScopeAnalysisPass::process(MatchExprAST* node) {
  for (auto &match_case : node->match_cases_) {
    match_case->run_pass(this);
  }
}

// CallExprAST
void ScopeAnalysisPass::process(CallExprAST* node) {
  node->ends_scope_ = true;
}

// SizeofExprAST
void ScopeAnalysisPass::process(SizeofExprAST* node) {
  node->ends_scope_ = true;
}

// PtrOffsetExprAST
void ScopeAnalysisPass::process(PtrOffsetExprAST* node) {
  node->ends_scope_ = true;
}

// PrototypeAST
void ScopeAnalysisPass::process(PrototypeAST* node) {
}

// FunctionAST
void ScopeAnalysisPass::process(FunctionAST* node) {
  node->Body->run_pass(this);
}

// TypeAST
void ScopeAnalysisPass::process(TypeAST* node) {
}

// TypeclassAST
void ScopeAnalysisPass::process(TypeclassAST* node) {
  for (auto &impl : node->impls) {
    impl->run_pass(this);
  }
}

// TypeclassImplAST
void ScopeAnalysisPass::process(TypeclassImplAST* node) {
  for (auto &method_entry : node->methods_) {
    auto &method = method_entry.second;
    method->run_pass(this);
  }
}

} // namespace bon
