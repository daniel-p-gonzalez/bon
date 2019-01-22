/*----------------------------------------------------------------------------*\
|*
|* Parser - responsible for building the AST
|*
L*----------------------------------------------------------------------------*/

#pragma once
#include "bonAST.h"
#include "bonTokenizer.h"
#include "bonModuleState.h"

#include <string>
#include <map>
#include <functional>
#include <memory>

namespace bon {

class Parser {
private:
  // precedence for binary operators
  std::map<Token, int> binop_precedence_;
  std::map<std::string, VariableExprAST*> vars_in_scope_;
  Tokenizer tokenizer_;
  ModuleState &state_;
  // stack of scopes for name mangling
  std::vector<std::string> scope_stack_;
  void update_tok_position();
  bool is_unary_op(Token op);
  // main parse loop
  void parse();

public:
  Parser(ModuleState &state);

  void parse_file(std::string filename);

  int get_operator_precedence();

  void reset_tokenizer();
  // Token consume();
  // Token peak();
  size_t line_number();
  size_t column();

  // parse all expression types
  std::unique_ptr<ExprAST> parse_expression();
  // literal floating point number
  std::unique_ptr<ExprAST> parse_number_expr();
  // literal integer number
  std::unique_ptr<ExprAST> parse_integer_expr();
  // literal string
  std::unique_ptr<ExprAST> parse_string_expr();
  // literal boolean "true" "false"
  std::unique_ptr<ExprAST> parse_bool_expr();
  // literal unit type "()"
  std::unique_ptr<ExprAST> parse_unit_expr();
  // '(' expression ')'
  std::unique_ptr<ExprAST> parse_paren_expr();
  // Variant value constructor
  // e.g. Some(5)
  std::unique_ptr<ExprAST> parse_value_constructor_expr();
  // variable reference, or function call
  std::unique_ptr<ExprAST> parse_identifier_expr();
  std::unique_ptr<ExprAST> parse_match_expr();
  // 'if' expression 'then' expression 'else' expression
  std::unique_ptr<ExprAST> parse_if_expr();
  std::unique_ptr<ExprAST> parse_list_item(std::unique_ptr<ExprAST> head);
  std::unique_ptr<ExprAST> parse_list_constructor();
  // parse primary expression
  std::unique_ptr<ExprAST> parse_primary();
  std::unique_ptr<ExprAST> parse_unary();
  // parse binary operator expression
  std::unique_ptr<ExprAST> parse_binop(int left_precedence,
                                       std::unique_ptr<ExprAST> LHS);
  // parse function prototype
  std::unique_ptr<PrototypeAST> parse_prototype();
  // parse type definition
  std::unique_ptr<TypeAST> parse_type();
  // parse typeclass definition
  std::unique_ptr<TypeclassAST> parse_typeclass();
  // parse typeclass implementation
  std::unique_ptr<TypeclassImplAST> parse_typeclass_impl();
  // parse function definition
  std::unique_ptr<FunctionAST> parse_definition();
  // parse top-level expression
  std::unique_ptr<FunctionAST> parse_toplevel_expr();
  // 'cdef' prototype
  std::unique_ptr<PrototypeAST> parse_extern();
  // parse import (returns filename)
  std::string parse_import(std::string current_filename);
};

} // namespace bon
