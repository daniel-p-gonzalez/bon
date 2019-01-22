/*----------------------------------------------------------------------------*\
|*
|* Abstract Syntax Tree (AST) nodes which are the output of bon::Parser
|*
L*----------------------------------------------------------------------------*/
#include "bonAST.h"
#include "bonCompilerPass.h"

#include <sstream>

namespace bon {

ExprAST::ExprAST(size_t line_num, size_t column_num)
  : line_num_(line_num), column_num_(column_num), is_boxed_(false) {
}

std::string ExprAST::getName() {
  return "";
}

void ExprAST::push_type_environment() {}
void ExprAST::pop_type_environment() {}

NumberExprAST::NumberExprAST(size_t line_num, size_t column_num, double Val)
  : ExprAST(line_num, column_num), Val(Val) {
    type_var_ = FloatType;
}

IntegerExprAST::IntegerExprAST(size_t line_num, size_t column_num, int64_t Val)
  : ExprAST(line_num, column_num), Val(Val) {
    type_var_ = IntType;
}

StringExprAST::StringExprAST(size_t line_num, size_t column_num,
                             std::string Val)
  : ExprAST(line_num, column_num), Val(Val) {
    type_var_ = StringType;
}

BoolExprAST::BoolExprAST(size_t line_num, size_t column_num, bool Val)
  : ExprAST(line_num, column_num), Val(Val) {
    type_var_ = BoolType;
}

UnitExprAST::UnitExprAST(size_t line_num, size_t column_num)
  : ExprAST(line_num, column_num) {
    type_var_ = UnitType;
}

VariableExprAST::VariableExprAST(size_t line_num, size_t column_num,
                                 const std::string &Name)
  : ExprAST(line_num, column_num), Name(Name) {
    type_var_ = new TypeVariable();
}

std::string VariableExprAST::getName() {
  return Name;
}

ValueConstructorExprAST::ValueConstructorExprAST(
                                size_t line_num,
                                size_t column_num,
                                const std::string &constructor,
                                std::vector<std::unique_ptr<ExprAST>> tcon_args)
  : ExprAST(line_num, column_num), constructor_(constructor),
    tcon_args_(std::move(tcon_args)) {
  type_var_ = new TypeVariable();
}

UnaryExprAST::UnaryExprAST(size_t line_num, size_t column_num, Token Opcode,
                           std::unique_ptr<ExprAST> Operand)
  : ExprAST(line_num, column_num), Opcode(Opcode), Operand(std::move(Operand)) {
  type_var_ = new TypeVariable();
}

BinaryExprAST::BinaryExprAST(size_t line_num, size_t column_num, Token Op,
                             std::unique_ptr<ExprAST> LHS,
                             std::unique_ptr<ExprAST> RHS)
  : ExprAST(line_num, column_num), Op(Op),
    LHS(std::move(LHS)), RHS(std::move(RHS)) {
  type_var_ = new TypeVariable();
  switch (Op) {
    case tok_add:
    case tok_sub:
    case tok_mul:
    case tok_div:
    case tok_assign:
    case tok_dot:
    case tok_concat:
    case tok_cons:
        inherit_child_type_ = true;
        break;
    case tok_gt:
    case tok_lt:
    case tok_eq:
    case tok_gteq:
    case tok_lteq:
        unify(type_var_, BoolType);
        // fall-through
    default:
        inherit_child_type_ = false;
        break;
  }
}

IfExprAST::IfExprAST(size_t line_num, size_t column_num,
                     std::unique_ptr<ExprAST> Cond,
                     std::unique_ptr<ExprAST> Then,
                     std::unique_ptr<ExprAST> Else)
  : ExprAST(line_num, column_num), Cond(std::move(Cond)), Then(std::move(Then)),
    Else(std::move(Else)) {
  type_var_ = new TypeVariable();
}

MatchCaseExprAST::MatchCaseExprAST(size_t line_num, size_t column_num,
                                   std::unique_ptr<ExprAST> condition,
                                   std::unique_ptr<ExprAST> body)
  : ExprAST(line_num, column_num), condition_(std::move(condition)),
    body_(std::move(body)) {
  type_var_ = new TypeVariable();
}

MatchExprAST::MatchExprAST(
                     size_t line_num, size_t column_num,
                     std::unique_ptr<ExprAST> pattern,
                     std::vector<std::unique_ptr<MatchCaseExprAST>> match_cases)
  : ExprAST(line_num, column_num), pattern_(std::move(pattern)),
    match_cases_(std::move(match_cases)) {
  type_var_ = new TypeVariable();
}

CallExprAST::CallExprAST(size_t line_num, size_t column_num,
                         const std::string &Callee,
                         std::vector<std::unique_ptr<ExprAST>> Args)
  : ExprAST(line_num, column_num), Callee(Callee), Args(std::move(Args)) {
  type_var_ = new TypeVariable();
}

PrototypeAST::PrototypeAST(size_t line_num, size_t column_num,
                           const std::string &Name,
                           std::vector<std::string> Args,
                           std::vector<TypeVariable*> ArgTypes,
                           TypeVariable* ret_type)
  : Name(Name), Args(std::move(Args)), ret_type_(ret_type),
    line_num_(line_num), column_num_(column_num) {
  type_var_ = build_function_type(ArgTypes, ret_type);
}

const std::string& PrototypeAST::getName() const {
  return Name;
}

FunctionAST::FunctionAST(size_t line_num, size_t column_num,
                         std::unique_ptr<PrototypeAST> Proto,
                         std::unique_ptr<ExprAST> Body, ExprAST* last_expr)
  : Proto(std::move(Proto)), Body(std::move(Body)),
    last_expr_(last_expr), line_num_(line_num), column_num_(column_num) {
}

TypeVariable* FunctionAST::type_var() {
  return Proto->type_var_;
}

TypeAST::TypeAST(std::string name, size_t line_num, size_t column_num,
                 TypeVariable* type_var)
  : name_(name), type_var_(type_var),
    line_num_(line_num), column_num_(column_num) {
}

TypeclassAST::TypeclassAST(std::string name, size_t line_num, size_t column_num,
                           std::vector<std::string> params,
                           TypeEnv &param_types,
                           TypeMap &methods)
  : name_(name), line_num_(line_num), column_num_(column_num),
    params_(params), param_types_(param_types), methods_(methods) {
}

TypeclassImplAST::TypeclassImplAST(std::string class_name,
                                   size_t line_num, size_t column_num,
                                   TypeEnv &param_types, TypeMap &method_types,
                                   std::map<std::string,
                                   std::unique_ptr<FunctionAST>> methods)
  : class_name_(class_name), line_num_(line_num), column_num_(column_num),
    param_types_(param_types), method_types_(method_types),
    methods_(std::move(methods)) {
}

bool ExprAST::is_boxed() {
  if (is_boxed_) {
    return is_boxed_;
  }

  if (is_boxed_type(type_var_)) {
    is_boxed_ = true;
  }
  else {
    is_boxed_ = false;
  }
  return is_boxed_;
}

bool UnitExprAST::is_boxed() {
  return false;
}


void ValueConstructorExprAST::push_type_environment() {
  push_environment(type_env_);
}

void ValueConstructorExprAST::pop_type_environment() {
  pop_environment();
}


std::string UnaryExprAST::get_mangled_name() {
  auto Callee = std::string("unary") + Tokenizer::token_type(Opcode);
  std::stringstream mangled_name;
  mangled_name << ":" << line_num_
               << ":" << Callee << ":" << static_cast<const void*>(this);
  return mangled_name.str();
}

std::string CallExprAST::get_mangled_name() {
  std::stringstream mangled_name;
  mangled_name << ":" << line_num_
               << ":" << Callee << ":" << static_cast<const void*>(this);
  return mangled_name.str();
}

void NumberExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void IntegerExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void StringExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void BoolExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void UnitExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void VariableExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void ValueConstructorExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void UnaryExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void BinaryExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void IfExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void MatchCaseExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void MatchExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void CallExprAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void PrototypeAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void FunctionAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void TypeAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void TypeclassAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

void TypeclassImplAST::run_pass(CompilerPass* pass) {
  pass->process(this);
}

} // namespace bon
