/*----------------------------------------------------------------------------*\
|*
|* Base class for a compiler pass (AST walker)
|*
L*----------------------------------------------------------------------------*/
#pragma once
#include "bonAST.h"

namespace bon {

class CompilerPass {
public:
  virtual void process(NumberExprAST* node) = 0;
  virtual void process(IntegerExprAST* node) = 0;
  virtual void process(StringExprAST* node) = 0;
  virtual void process(BoolExprAST* node) = 0;
  virtual void process(UnitExprAST* node) = 0;
  virtual void process(VariableExprAST* node) = 0;
  virtual void process(ValueConstructorExprAST* node) = 0;
  virtual void process(UnaryExprAST* node) = 0;
  virtual void process(BinaryExprAST* node) = 0;
  virtual void process(IfExprAST* node) = 0;
  virtual void process(WhileExprAST* node) = 0;
  virtual void process(MatchCaseExprAST* node) = 0;
  virtual void process(MatchExprAST* node) = 0;
  virtual void process(CallExprAST* node) = 0;
  virtual void process(PrototypeAST* node) = 0;
  virtual void process(SizeofExprAST* node) = 0;
  virtual void process(PtrOffsetExprAST* node) = 0;
  virtual void process(FunctionAST* node) = 0;
  virtual void process(TypeAST* node) = 0;
  virtual void process(TypeclassAST* node) = 0;
  virtual void process(TypeclassImplAST* node) = 0;
};

} // namespace bon
