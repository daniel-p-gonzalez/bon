/*----------------------------------------------------------------------------*\
|*
|* Abstract Syntax Tree (AST) nodes which are the output of bon::Parser
|*
L*----------------------------------------------------------------------------*/
#pragma once
#include "bonTypesystem.h"
#include "bonTokenizer.h"
// TODO: remove this dependency
#include "bonLLVM.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

namespace bon {

class CompilerPass;

struct ExprAST {
  TypeVariable* type_var_;
  size_t line_num_;
  size_t column_num_;
  bool is_boxed_;
  // for tracking lifetimes
  bool ends_scope_;

  ExprAST(size_t line_num, size_t column_num);
  virtual ~ExprAST() = default;

  virtual void run_pass(CompilerPass* pass) = 0;

  virtual bool is_boxed();
  virtual void set_as_lvalue() {}

  virtual std::string getName();
  virtual void push_type_environment();
  virtual void pop_type_environment();
};
typedef std::unique_ptr<ExprAST> ExprASTPtr;

// ast node for numeric literals like "1.0"
struct NumberExprAST : public ExprAST {
  double Val;

  NumberExprAST(size_t line_num, size_t column_num, double Val);
  void run_pass(CompilerPass* pass) override;
};
typedef std::unique_ptr<NumberExprAST> NumberExprASTPtr;

// ast node for integer literals like "61"
struct IntegerExprAST : public ExprAST {
  int64_t Val;

  IntegerExprAST(size_t line_num, size_t column_num, int64_t Val);
  void run_pass(CompilerPass* pass) override;
};
typedef std::unique_ptr<IntegerExprAST> IntegerExprASTPtr;

// TODO: primitive numeric types -
//   int=BigInt, float=double
//   i8, i16, i32, i64
//   u8, u16, u32, u64
//   f32, f64

// // ast node for numeric literals like "1.0"
// struct FloatExprAST : public ExprAST {
//   double Val;

//   FloatExprAST(size_t line_num, size_t column_num, double Val);
//   void run_pass(CompilerPass* pass) override;
// // };

// ast node for string literals like "foo bar"
struct StringExprAST : public ExprAST {
  std::string Val;

  StringExprAST(size_t line_num, size_t column_num, std::string Val);
  void run_pass(CompilerPass* pass) override;
};
typedef std::unique_ptr<StringExprAST> StringExprASTPtr;

// ast node for boolean literals like "True"
struct BoolExprAST : public ExprAST {
  bool Val;

  BoolExprAST(size_t line_num, size_t column_num, bool Val);
  void run_pass(CompilerPass* pass) override;
};
typedef std::unique_ptr<BoolExprAST> BoolExprASTPtr;

// ast node for literal unit type "()"
struct UnitExprAST : public ExprAST {
  UnitExprAST(size_t line_num, size_t column_num);
  void run_pass(CompilerPass* pass) override;
  bool is_boxed() override;
};
typedef std::unique_ptr<UnitExprAST> UnitExprASTPtr;

// ast node for referencing a variable, like "a"
struct VariableExprAST : public ExprAST {
  std::string Name;

  VariableExprAST(size_t line_num, size_t column_num, const std::string &Name);
  void run_pass(CompilerPass* pass) override;
  std::string getName() override;
};
typedef std::unique_ptr<VariableExprAST> VariableExprASTPtr;

struct ValueConstructorExprAST : public ExprAST {
  std::string constructor_;
  std::vector<ExprASTPtr> tcon_args_;
  TypeEnv type_env_;
  bool heap_alloc_;

  ValueConstructorExprAST(size_t line_num, size_t column_num,
                          const std::string &constructor,
                          std::vector<ExprASTPtr> tcon_args,
                          bool heap_alloc);
  void run_pass(CompilerPass* pass) override;
  void push_type_environment() override;
  void pop_type_environment() override;
};
typedef std::unique_ptr<ValueConstructorExprAST> ValueConstructorExprASTPtr;

// ast node for a unary operator
struct UnaryExprAST : public ExprAST {
  Token Opcode;
  ExprASTPtr Operand;
  TypeEnv Env;

  UnaryExprAST(size_t line_num, size_t column_num, Token Opcode,
               ExprASTPtr Operand);
  void run_pass(CompilerPass* pass) override;
  std::string get_mangled_name();
};
typedef std::unique_ptr<UnaryExprAST> UnaryExprASTPtr;

// ast node for a binary operator
struct BinaryExprAST : public ExprAST {
  Token Op;
  ExprASTPtr LHS, RHS;
  bool inherit_child_type_;
  bool is_lvalue;
  TypeEnv Env;

  BinaryExprAST(size_t line_num, size_t column_num, Token Op, ExprASTPtr LHS,
                ExprASTPtr RHS);
  void run_pass(CompilerPass* pass) override;

  void push_type_environment() override;
  void pop_type_environment() override;
  void set_as_lvalue() override;
};
typedef std::unique_ptr<BinaryExprAST> BinaryExprASTPtr;

// ast node for if/then/else
struct IfExprAST : public ExprAST {
  ExprASTPtr Cond, Then, Else;

  IfExprAST(size_t line_num, size_t column_num,
            ExprASTPtr Cond, ExprASTPtr Then, ExprASTPtr Else);
  void run_pass(CompilerPass* pass) override;
};
typedef std::unique_ptr<IfExprAST> IfExprASTPtr;

// ast node for while loop
struct WhileExprAST : public ExprAST {
  ExprASTPtr condition_, body_;

  WhileExprAST(size_t line_num, size_t column_num,
            ExprASTPtr condition, ExprASTPtr body);
  void run_pass(CompilerPass* pass) override;
};
typedef std::unique_ptr<WhileExprAST> WhileExprASTPtr;

struct MatchCaseExprAST : public ExprAST {
  ExprASTPtr condition_, body_;

  MatchCaseExprAST(size_t line_num, size_t column_num,
                   ExprASTPtr condition, ExprASTPtr body);
  void run_pass(CompilerPass* pass) override;
};
typedef std::unique_ptr<MatchCaseExprAST> MatchCaseExprASTPtr;

struct MatchExprAST : public ExprAST {
  ExprASTPtr pattern_;
  std::vector<MatchCaseExprASTPtr> match_cases_;

  MatchExprAST(size_t line_num, size_t column_num, ExprASTPtr pattern,
               std::vector<MatchCaseExprASTPtr> match_cases);
  void run_pass(CompilerPass* pass) override;
};
typedef std::unique_ptr<MatchExprAST> MatchExprASTPtr;

// ast node for function calls
struct CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprASTPtr> Args;
  TypeEnv Env;

  CallExprAST(size_t line_num, size_t column_num, const std::string &Callee,
              std::vector<ExprASTPtr> Args);
  void run_pass(CompilerPass* pass) override;
  // Value *codegen() override;
  std::string get_mangled_name();
};
typedef std::unique_ptr<CallExprAST> CallExprASTPtr;

// ast node for sizeof builtin
struct SizeofExprAST : public ExprAST {
  ExprASTPtr arg_;

  SizeofExprAST(size_t line_num, size_t column_num, ExprASTPtr arg);
  void run_pass(CompilerPass* pass) override;
};
typedef std::unique_ptr<SizeofExprAST> SizeofExprASTPtr;

// ast node for ptr_offset builtin
struct PtrOffsetExprAST : public ExprAST {
  ExprASTPtr arg_;
  ExprASTPtr offset_;
  bool is_lvalue;

  PtrOffsetExprAST(size_t line_num, size_t column_num, ExprASTPtr arg,
                   ExprASTPtr offset);
  void run_pass(CompilerPass* pass) override;

  void set_as_lvalue() override {
    is_lvalue = true;
  }
};
typedef std::unique_ptr<PtrOffsetExprAST> PtrOffsetExprASTPtr;


// "prototype" for a function,
// which captures its name, and its argument names (thus implicitly the number
// of arguments the function takes)
struct PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
  // does arg[i] transfer ownership to the function
  std::vector<bool> arg_owned_;
  TypeVariable* type_var_;
  TypeVariable* ret_type_;
  size_t line_num_;
  size_t column_num_;

  PrototypeAST(size_t line_num, size_t column_num, const std::string &Name,
               std::vector<std::string> Args,
               std::vector<TypeVariable*> ArgTypes,
               std::vector<bool> arg_owned,
               TypeVariable* ret_type);
  void run_pass(CompilerPass* pass);
  const std::string& getName() const;
};
typedef std::unique_ptr<PrototypeAST> PrototypeASTPtr;

// function body ast node
struct FunctionAST {
  PrototypeASTPtr Proto;
  ExprASTPtr Body;
  std::vector<ExprAST*> Params;
  // functions called from here which must be generated first
  std::vector<CallExprAST*> dependencies_;
  TypeEnv type_env_;
  TypeEnv param_name_to_tvar_;

  // typeclass that this function belongs to
  std::string typeclass;

  ExprAST* last_expr_;
  size_t line_num_;
  size_t column_num_;

  FunctionAST(size_t line_num, size_t column_num, PrototypeASTPtr Proto,
              ExprASTPtr Body, ExprAST* last_expr,
              std::vector<CallExprAST*> &dependencies);
  void run_pass(CompilerPass* pass);
  TypeVariable* type_var();
};
typedef std::unique_ptr<FunctionAST> FunctionASTPtr;

struct TypeAST {
  std::string name_;
  // array of polymorphic parameters
  std::vector<std::string> type_parameters_;
  TypeEnv constructors_;
  TypeVariable* type_var_;
  size_t line_num_;
  size_t column_num_;

  TypeAST(std::string name, size_t line_num, size_t column_num,
          TypeVariable* type_var);
  void run_pass(CompilerPass* pass);
};
typedef std::unique_ptr<TypeAST> TypeASTPtr;

struct TypeclassImplAST;
typedef std::unique_ptr<TypeclassImplAST> TypeclassImplASTPtr;

struct TypeclassAST {
  // typeclass name e.g. "Num" in class Num<T>
  std::string name_;
  // array of type variable names e.g. ["T"] in class Num<T>
  std::vector<std::string> params_;
  // map from type variable name to type variable
  TypeEnv param_types_;
  // type variables for methods within the typeclass
  //  e.g. "operator+" in class Num
  TypeMap methods_;
  std::vector<TypeclassImplASTPtr> impls;
  size_t line_num_;
  size_t column_num_;

  TypeclassAST(std::string name, size_t line_num, size_t column_num,
               std::vector<std::string> params, TypeEnv &param_types,
               TypeMap &methods);
  void run_pass(CompilerPass* pass);
};
typedef std::unique_ptr<TypeclassAST> TypeclassASTPtr;

struct TypeclassImplAST {
  // name of typeclass being implemented for this type
  std::string class_name_;
  // map from type variable name to concrete type
  //  e.g. IntType to substitute type "T" in class Num<T>
  TypeEnv param_types_;
  // concrete function types for methods within the typeclass
  //  e.g. the type of operator+(lhs:int, rhs:int)->int
  TypeMap method_types_;
  std::map<std::string, FunctionASTPtr> methods_;
  std::map<std::string, Function*> method_funcs_;
  size_t line_num_;
  size_t column_num_;

  TypeclassImplAST(std::string class_name, size_t line_num, size_t column_num,
                   TypeEnv &param_types, TypeMap &method_types_,
                   std::map<std::string, FunctionASTPtr> methods);
  void run_pass(CompilerPass* pass);
};

} // namespace bon
