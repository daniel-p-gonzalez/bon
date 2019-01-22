/*----------------------------------------------------------------------------*\
|*
|* Simple example AST walker (just prints AST node types)
|*
L*----------------------------------------------------------------------------*/
#include "bonDebugASTPass.h"

namespace bon {

// floating point number
void DebugASTPass::process(NumberExprAST* node) {
  std::cout << "got a float" << std::endl;
}

// integer number
void DebugASTPass::process(IntegerExprAST* node) {
  std::cout << "got an int" << std::endl;
}

// string
void DebugASTPass::process(StringExprAST* node) {
  std::cout << "got a string" << std::endl;
}

// boolean
void DebugASTPass::process(BoolExprAST* node) {
  std::cout << "got a bool" << std::endl;
}

// unit ()
void DebugASTPass::process(UnitExprAST* node) {
  std::cout << "got a unit" << std::endl;
}

// variable
void DebugASTPass::process(VariableExprAST* node) {
  std::cout << "got a variable" << std::endl;
}

// value constructor
void DebugASTPass::process(ValueConstructorExprAST* node) {
  std::cout << "got a value constructor" << std::endl;
  for (auto &arg : node->tcon_args_) {
    arg->run_pass(this);
  }
}

// unary op
void DebugASTPass::process(UnaryExprAST* node) {
  std::cout << "got a unary" << std::endl;
  node->Operand->run_pass(this);
}

// binop
void DebugASTPass::process(BinaryExprAST* node) {
  std::cout << "got a binary op" << std::endl;
  node->LHS->run_pass(this);
  node->RHS->run_pass(this);
}

// if expression
void DebugASTPass::process(IfExprAST* node) {
  std::cout << "got a if" << std::endl;
  node->Cond->run_pass(this);
  node->Then->run_pass(this);
  node->Else->run_pass(this);
}

// match case
void DebugASTPass::process(MatchCaseExprAST* node) {
  std::cout << "got a match case" << std::endl;
  node->condition_->run_pass(this);
  node->body_->run_pass(this);
}

// match
void DebugASTPass::process(MatchExprAST* node) {
  std::cout << "got a match expression" << std::endl;
  node->pattern_->run_pass(this);
  for (auto &match_case : node->match_cases_) {
    match_case->run_pass(this);
  }
}

// function call
void DebugASTPass::process(CallExprAST* node) {
  std::cout << "got a call" << std::endl;
  for (auto &arg : node->Args) {
    arg->run_pass(this);
  }
}

// prototype
void DebugASTPass::process(PrototypeAST* node) {
  std::cout << "got a proto" << std::endl;
}

// function body
void DebugASTPass::process(FunctionAST* node) {
  std::cout << "got a function" << std::endl;
  node->Body->run_pass(this);
  for (auto &param : node->Params) {
    param->run_pass(this);
  }
}

// type definition
void DebugASTPass::process(TypeAST* node) {
  std::cout << "got a type" << std::endl;
}

// typeclass definition
void DebugASTPass::process(TypeclassAST* node) {
  std::cout << "got a typeclass" << std::endl;
}

// typeclass implementation
void DebugASTPass::process(TypeclassImplAST* node) {
  std::cout << "got a typeclass impl" << std::endl;
}

} // namespace bon
