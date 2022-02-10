/*----------------------------------------------------------------------------*\
|*
|* Tokenizer - text to token processing for use by bon::Parser
|*
L*----------------------------------------------------------------------------*/

#include "bonTokenizer.h"
#include "bonLogger.h"

#include <iostream>
#include <cstdio>
#include <cctype>
#include <map>

namespace bon {

int Tokenizer::next_char() {
  ++col_;
  int chr = getchar();
  if (chr == '\n') {
    ++pos_.line;
    col_ = -1;
  }

  return chr;
}

void Tokenizer::eat_comment() {
  // eat until end of line or file.
  do
    last_char_ = next_char();
  while (last_char_ != EOF && last_char_ != '\n' && last_char_ != '\r');

  at_line_start_ = true;
}

Token Tokenizer::eof_token() {
  if (current_token_ == tok_eof) {
    // eat eof if we've already processed it
    // TODO: this might cause problems on compile error
    last_char_ = next_char();
    return next_token();
  }
  return tok_eof;
}

Token Tokenizer::next_token() {
  if (last_char_ == -1) {
    last_char_ = next_char();
  }

  bool blankline = false;

  if (!token_cache.empty()) {
    Token next_tok = token_cache.back();
    token_cache.pop_back();

    if (next_tok == tok_dedent) {
      if (!skip_next_dedent_) {
        return tok_dedent;
      }
      else {
        skip_next_dedent_ = false;
        return next_token();
      }
    }
    else {
      return next_tok;
    }
  }

  if (last_char_ == '\n') {
    at_line_start_ = true;
  }
  bool edge_case = false;

  if (at_line_start_) {
    int col = 0;
    at_line_start_ = false;
    while (last_char_ == '\n') {
      last_char_ = next_char();
    }
    while (true) {
      if (last_char_ == ' ') {
        ++col;
      }
      else if (last_char_ == '\t') {
        col += 4;
      }
      else {
        break;
      }
      last_char_ = next_char();
    }
    if (last_char_ == '#') {
      eat_comment();
      if (last_char_ == EOF) {
        return eof_token();
      }
      return next_token();
    }
    if (last_char_ == '\n') {
      at_line_start_ = true;
      while (last_char_ == '\n') {
        last_char_ = next_char();
      }
      return next_token();
    }
    if (col > indent_sizes_[indent_level_]) {
      if (indent_level_ >= MAX_INDENTS+1) {
        bon::logger.set_line_column(pos_.line, col);
        bon::logger.error("error", "Exceeded max indent level (20)");
      }
      ++indent_level_;
      indent_sizes_[indent_level_] = col;
      return tok_indent;
    }
    else if (col < indent_sizes_[indent_level_]) {
      --indent_level_;
      while (col < indent_sizes_[indent_level_] && indent_level_ > 0) {
        // token_cache.push_back(tok_dedent);
        --indent_level_;
        token_cache.push_back(tok_dedent);
      }
      if (indent_sizes_[indent_level_] != col) {
        edge_case = true;
        // TokError("unindent does not match any outer indentation level.");
        // minimize future complaining about the same error for case
        //  where an entire block is misaligned
        indent_sizes_[indent_level_] = col;
      }
      if (!skip_next_dedent_) {
        return tok_dedent;
      }
      else {
        skip_next_dedent_ = false;
        return next_token();
      }
    }
  }
  else {
    // Skip any whitespace.
    while (isspace(last_char_)) {
      if (last_char_ == '\n') {
        at_line_start_ = true;
      }
      else {
        at_line_start_ = false;
      }
      last_char_ = next_char();
    }
  }

  pos_.column = col_ == -1 ? 0 : col_;

  // string literal
  if (last_char_ == '"') {
    identifier_ = "";
    bool last_was_escape = false;
    while ((last_char_ = next_char()) != '"' || last_was_escape) {
      if (last_was_escape) {
        if (last_char_ == 'n') {
          identifier_ += '\n';
          last_was_escape = false;
          continue;
        }
        else if (last_char_ == '\"') {
          identifier_ += '\"';
          last_was_escape = false;
          continue;
        }
        else {
          bon::logger.set_line_column(pos_.line, col_);
          bon::logger.error("error", "unrecognized escape sequence");
        }
      }
      if (last_char_ == '\\') {
        last_was_escape = true;
        continue;
      }
      else {
        last_was_escape = false;
      }
      identifier_ += last_char_;
    }
    // swallow closing '"'
    last_char_ = next_char();
    return tok_string;
  }

  // identifier [_a-zA-Z][_a-zA-Z0-9]*
  if (isalpha(last_char_) || last_char_ == '_') {
    at_line_start_ = false;
    identifier_ = last_char_;
    while (isalnum((last_char_ = next_char())) || last_char_ == '_')
      identifier_ += last_char_;

    if (identifier_ == "import")
      return tok_import;
    if (identifier_ == "def")
      return tok_def;
    if (identifier_ == "new")
      return tok_new;
    if (identifier_ == "typeclass")
      return tok_typeclass;
    if (identifier_ == "impl")
      return tok_impl;
    if (identifier_ == "cdef")
      return tok_extern;
    if (identifier_ == "return")
      return tok_return;
    if (identifier_ == "if")
      return tok_if;
    if (identifier_ == "then")
      return tok_then;
    if (identifier_ == "while")
      return tok_while;
    if (identifier_ == "do")
      return tok_do;
    if (identifier_ == "else")
      return tok_else;
    if (identifier_ == "end")
      return tok_end;
    if (identifier_ == "and")
      return tok_and;
    if (identifier_ == "or")
      return tok_or;
    if (identifier_ == "class")
      return tok_class;
    if (identifier_ == "sizeof")
      return tok_sizeof;
    if (identifier_ == "ptr_offset")
      return tok_ptr_offset;
    if (identifier_ == "match")
      return tok_match;
    if (identifier_ == "struct")
      return tok_struct;
    if (identifier_ == "operator")
      return tok_binary;
    if (identifier_ == "unary")
      return tok_unary;
    if (identifier_ == "true") {
      bool_val_ = true;
      return tok_true;
    }
    if (identifier_ == "false") {
      bool_val_ = false;
      return tok_false;
    }
    if (isupper(identifier_[0])) {
      return tok_identifier;
    }
    return tok_identifier;
  }

  // number literal
  bool is_float = false;
  if (isdigit(last_char_) || last_char_ == '.') {
    at_line_start_ = false;
    std::string num_str;
    do {
      if (last_char_ == '.') {
        if (is_float) {
          bon::logger.set_line_column(pos_.line, col_);
          bon::logger.error("error", "malformed floating point number");
        }
        is_float = true;
      }
      num_str += last_char_;
      last_char_ = next_char();
    } while (isdigit(last_char_) || last_char_ == '.');

    if (num_str == ".") {
      return tok_dot;
    }
    else if (is_float) {
      num_val_ = strtod(num_str.c_str(), nullptr);
      return tok_number;
    }
    else {
      int_val_ = strtoll(num_str.c_str(), nullptr, 10);
      return tok_integer;
    }
  }

  // comment
  if (last_char_ == '#') {
    eat_comment();
    if (last_char_ != EOF) {
      return next_token();
    }
  }

  // check for end of file
  if (last_char_ == EOF) {
    return eof_token();
  }

  if (last_char_ == '\n') {
    bon::logger.error("internal error", "unexpected newline");
    at_line_start_ = true;
    last_char_ = next_char();
    return next_token();
  }

  // Cons operator
  if (last_char_ == ':') {
    last_char_ = next_char();
    if (last_char_ == ':') {
      last_char_ = next_char();
      return tok_cons;
    }
    return tok_colon;
  }

  // concat operator
  if (last_char_ == '+') {
    last_char_ = next_char();
    if (last_char_ == '+') {
      last_char_ = next_char();
      return tok_concat;
    }
    return tok_add;
  }

  // exponentiation
  if (last_char_ == '*') {
    last_char_ = next_char();
    if (last_char_ == '*') {
      last_char_ = next_char();
      return tok_pow;
    }
    return tok_mul;
  }

  // arrow
  if (last_char_ == '-') {
    last_char_ = next_char();
    if (last_char_ == '>') {
      last_char_ = next_char();
      return tok_arrow;
    }
    return tok_sub;
  }

  // greater than or equal to '>='
  if (last_char_ == '>') {
    last_char_ = next_char();
    if (last_char_ == '=') {
      last_char_ = next_char();
      return tok_gteq;
    }
    else if(last_char_ == '>') {
      last_char_ = next_char();
      return tok_rshift;
    }
    return tok_gt;
  }

  // less than or equal to '<='
  if (last_char_ == '<') {
    last_char_ = next_char();
    if (last_char_ == '=') {
      last_char_ = next_char();
      return tok_lteq;
    }
    else if(last_char_ == '<') {
      last_char_ = next_char();
      return tok_lshift;
    }
    return tok_lt;
  }

  if (last_char_ == '!') {
    last_char_ = next_char();
    if (last_char_ == '=') {
      last_char_ = next_char();
      return tok_neq;
    }
    return tok_negate;
  }

  // double arrow '=>', or equality '=='
  if (last_char_ == '=') {
    last_char_ = next_char();
    if (last_char_ == '>') {
      last_char_ = next_char();
      return tok_double_arrow;
    }
    else if(last_char_ == '=') {
      last_char_ = next_char();
      return tok_eq;
    }
    return tok_assign;
  }

  // "[]" is syntactic sugar for empty vector constructor
  if (last_char_ == '[') {
    do
      last_char_ = next_char();
    while (last_char_ != EOF && isspace(last_char_));
    if (last_char_ == ']') {
      last_char_ = next_char();
      identifier_ = "vec";
      // token_cache is a stack, hence seemingly backwards {')', '('}
      token_cache.push_back(tok_rparen);
      token_cache.push_back(tok_lparen);
      return tok_identifier;
    }
    return tok_lbracket;
  }

  // unit type (TODO: is this necessary?)
  if (last_char_ == '(') {
    do
      last_char_ = next_char();
    while (last_char_ != EOF && isspace(last_char_));
    if (last_char_ == ')') {
      last_char_ = next_char();
      identifier_ = "()";
      return tok_unit;
    }
    return tok_lparen;
  }

  // otherwise, just return the character as its ascii value.
  int this_char = last_char_;
  last_char_ = next_char();
  static std::map<char, Token> token_map =
    {
      {'+', tok_add},
      {'-', tok_sub},
      {'*', tok_mul},
      {'/', tok_div},
      {'%', tok_rem},
      {'|', tok_bt_or},
      {'^', tok_bt_xor},
      {'&', tok_bt_and},
      {':', tok_colon},
      {'(', tok_lparen},
      {')', tok_rparen},
      {'[', tok_lbracket},
      {']', tok_rbracket},
      {',', tok_comma},
      {';', tok_sep},
      {'<', tok_lt},
      {'>', tok_gt},
      {'=', tok_assign}
    };
  auto tok = token_map.find(this_char);
  if (tok == token_map.end()) {
    return tok_unknown;
  }

  return tok->second;
}

Tokenizer::Tokenizer() {
  reset();
}

void Tokenizer::skip_next_dedent() {
  skip_next_dedent_ = true;
}

void Tokenizer::reset() {
  last_char_ = -1;
  pos_.line = 0;
  pos_.column = 0;
  col_ = 0;
  at_line_start_ = true;
  skip_next_dedent_ = false;
  token_cache.clear();
  indent_level_ = 0;
  for (size_t i = 0; i < MAX_INDENTS; ++i) {
    indent_sizes_[i] = 0;
  }
}

DocPosition Tokenizer::get_position() {
  return pos_;
}

void Tokenizer::override_position(DocPosition pos) {
  pos_.line = pos.line;
  pos_.column = pos.column;
}

// step to next token
Token Tokenizer::consume() {
  return current_token_ = next_token();
}

// check the type of the current token
Token Tokenizer::peak() {
  return current_token_;
}

std::string Tokenizer::token_type(Token token) {
  switch(token) {
    case tok_arrow:
      return "'->'";
    case tok_binary:
      return "'operator'";
    case tok_cons:
      return "'::'";
    case tok_concat:
      return "'++'";
    case tok_dedent:
      return "<unindent>";
    case tok_def:
      return "'def'";
    case tok_new:
      return "'new'";
    case tok_typeclass:
      return "'class'";
    case tok_impl:
      return "'impl'";
    case tok_double_arrow:
      return "'=>'";
    case tok_else:
      return "'else'";
    case tok_end:
      return "'end'";
    case tok_eof:
      return "<EOF>";
    case tok_extern:
      return "'cdef'";
    case tok_false:
      return "'false'";
    case tok_identifier:
      return "<identifier>";
    case tok_if:
      return "'if'";
    case tok_then:
      return "'then'";
    case tok_while:
      return "'while'";
    case tok_do:
      return "'do'";
    case tok_import:
      return "'import'";
    case tok_indent:
      return "<indent>";
    case tok_match:
      return "'match'";
    case tok_number:
      return "<float>";
    case tok_integer:
      return "<int>";
    case tok_add:
      return "'+'";
    case tok_sub:
      return "'-'";
    case tok_mul:
      return "'*'";
    case tok_div:
      return "'/'";
    case tok_rem:
      return "'%'";
    case tok_lshift:
      return "'<<'";
    case tok_rshift:
      return "'>>'";
    case tok_bt_or:
      return "'|'";
    case tok_bt_xor:
      return "'^'";
    case tok_bt_and:
      return "'&'";
    case tok_colon:
      return "':'";
    case tok_sep:
      return "';'";
    case tok_comma:
      return "','";
    case tok_lparen:
      return "'('";
    case tok_rparen:
      return "')'";
    case tok_lbracket:
      return "'['";
    case tok_rbracket:
      return "']'";
    case tok_pow:
      return "'**'";
    case tok_eq:
      return "'=='";
    case tok_neq:
      return "'!='";
    case tok_negate:
      return "'!'";
    case tok_gt:
      return "'>'";
    case tok_lt:
      return "'<'";
    case tok_gteq:
      return "'>='";
    case tok_lteq:
      return "'<='";
    case tok_assign:
      return "'='";
    case tok_and:
      return "'and'";
    case tok_or:
      return "'or'";
    case tok_return:
      return "'return'";
    case tok_string:
      return "<string>";
    case tok_struct:
      return "'struct'";
    case tok_true:
      return "'true'";
    case tok_class:
      return "'type'";
    case tok_unary:
      return "'unary'";
    case tok_unit:
      return "'()'";
    case tok_unknown:
    default:
      return "<unknown>";
  }
}

} // namespace bon
