/*----------------------------------------------------------------------------*\
|*
|* Tokenizer - text to token processing for use by bon::Parser
|*
L*----------------------------------------------------------------------------*/

#pragma once
#include "bonLogger.h"

#include <vector>
#include <string>

namespace bon {

enum Token {
  tok_eof = -1,

  // commands
  tok_def,
  tok_extern,
  tok_return,
  tok_import,
  tok_new,

  // primary
  tok_identifier,
  tok_number,
  tok_integer,
  tok_string,
  tok_true,
  tok_false,
  // TODO: this might be better handled in the parser instead of the lexer
  tok_unit,
  tok_lparen,
  tok_rparen,

  // block scoping
  tok_indent,
  tok_dedent,
  tok_end,

  // control flow
  tok_if,
  tok_then,
  tok_else,
  tok_while,
  tok_do,

  // binary ops
  tok_add,
  tok_sub,
  tok_mul,
  tok_div,
  tok_rem,
  tok_lshift,
  tok_rshift,
  tok_bt_or,
  tok_bt_xor,
  tok_bt_and,
  tok_gt,
  tok_lt,
  tok_eq,
  tok_neq,
  tok_negate,
  tok_gteq,
  tok_lteq,
  tok_concat,
  tok_pow,
  tok_cons,
  tok_assign,
  tok_and,
  tok_or,

  // separator i.e. ';'
  tok_sep,
  tok_comma,

  // typing
  tok_arrow,
  tok_colon,
  tok_type,
  tok_struct,
  tok_typeconstructor,
  tok_class,
  tok_impl,

  // builtin
  tok_sizeof,
  tok_ptr_offset,

  // objects
  tok_dot,

  // lists
  tok_lbracket,
  tok_rbracket,

  // pattern matching
  tok_match,
  tok_double_arrow,

  // operator overloading
  tok_binary,
  tok_unary,
  tok_unknown
};

class Tokenizer {
private:
  static constexpr size_t MAX_INDENTS = 20;
  std::vector<Token> token_cache;
  int indent_sizes_[MAX_INDENTS];
  int indent_level_;
  std::string identifier_;
  double num_val_;
  int64_t int_val_;
  bool bool_val_;
  DocPosition pos_;
  size_t col_;
  int last_char_;
  Token current_token_;
  bool at_line_start_;
  bool skip_next_dedent_;

  int next_char();
  Token next_token();

public:
  Tokenizer();
  void reset();
  DocPosition get_position();
  void override_position(DocPosition pos);

  size_t line_number() { return pos_.line; }
  size_t column() { return pos_.column; }

  double number_value() { return num_val_; }
  int64_t integer_value() { return int_val_; }
  std::string identifier() { return identifier_; }
  bool bool_value() { return bool_val_; }

  static std::string token_type(Token token);

  // step to next token
  Token consume();
  // check the type of the current token
  Token peak();
  void skip_next_dedent();
};

} // namespace bon
