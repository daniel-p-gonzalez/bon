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
  node->RHS->run_pass(this);
}

// IfExprAST
void ScopeAnalysisPass::process(IfExprAST* node) {
  node->Then->run_pass(this);
  node->Else->run_pass(this);
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
