/*----------------------------------------------------------------------------*\
|*
|* Parser - responsible for building the AST
|*
L*----------------------------------------------------------------------------*/

#include "bonParser.h"
#include "bonLogger.h"
#include "auto_scope.h"
#include "utils.h"

namespace bon {

Parser::Parser(ModuleState &state) : state_(state) {
  // set precedence for binary operators
  binop_precedence_[tok_assign] =  1;
  binop_precedence_[tok_or] = 3;
  binop_precedence_[tok_and] = 5;
  binop_precedence_[tok_bt_or] = 7;
  binop_precedence_[tok_bt_xor] = 8;
  binop_precedence_[tok_bt_and] = 9;
  binop_precedence_[tok_lt] = 10;
  binop_precedence_[tok_gt] = 10;
  binop_precedence_[tok_lteq] = 10;
  binop_precedence_[tok_gteq] = 10;
  binop_precedence_[tok_eq] = 10;
  binop_precedence_[tok_neq] = 10;
  binop_precedence_[tok_lshift] = 15;
  binop_precedence_[tok_rshift] = 15;
  binop_precedence_[tok_add] = 20;
  binop_precedence_[tok_sub] = 20;
  binop_precedence_[tok_mul] = 40;
  binop_precedence_[tok_div] = 40;
  binop_precedence_[tok_rem] = 40;
  binop_precedence_[tok_cons] = 40;
  binop_precedence_[tok_pow] = 50;
  binop_precedence_[tok_concat] = 60;
  binop_precedence_[tok_dot] = 70;
}

void Parser::parse() {
  // prime first token
  tokenizer_.consume();

  while (true) {
    switch (tokenizer_.peak()) {
    case tok_eof:
      return;
    case tok_sep:
      // eat optional semicolons.
      tokenizer_.consume();
      break;
    case tok_def:
      if (auto function_ast = parse_definition()) {
        auto func_name = function_ast->Proto->getName();
        state_.ordered_functions.push_back(function_ast.get());
        state_.all_functions[func_name] = std::move(function_ast);
        state_.function_names.push_back(func_name);
      }
      break;
    case tok_class:
      if (auto tclass = parse_typeclass()) {
        for (auto &method : tclass->methods_) {
          // add mapping from method name to typeclass
          state_.method_to_typeclass[method.first] = tclass->name_;
        }
        state_.typeclasses[tclass->name_] = std::move(tclass);
      }
      else {
        // logger.error("failed", "could not parse typeclass");
      }
      break;
    case tok_impl:
      if (auto tcls_impl = parse_typeclass_impl()) {
        // add implementation to typeclass
        auto & impls = state_.typeclasses[tcls_impl->class_name_]->impls;
        // allow overriding implementation
        impls.insert(impls.begin(), std::move(tcls_impl));
        // impls.push_back(std::move(tcls_impl));
      }
      else {
        // logger.error("failed", "could not parse type impl");
      }
      break;
    case tok_type:
      if (!parse_type()) {
        logger.error("failed", "could not parse type");
      }
      break;
    case tok_extern:
      if (auto proto_ast = parse_extern()) {
        // auto &proto_ref = *proto_ast;
        state_.function_protos[proto_ast->getName()] = std::move(proto_ast);
        // CodeGenPass code_gen_pass(state_);
        // proto_ref.run_pass(&code_gen_pass);
      }
      break;
    case tok_import:
      state_.filename = parse_import(state_.filename);
      logger.set_current_file(state_.filename);
      break;
    default:
      // parse top-level expression into an anonymous function
      if (auto function_ast = parse_toplevel_expr()) {
        state_.toplevel_expressions.push_back(std::move(function_ast));
      }
      break;
    }
  }
}

void Parser::parse_file(std::string filename) {
  reset_tokenizer();

  state_.filename = filename;
  logger.config(20, 100);
  logger.set_current_file(state_.filename);

  if (filename != "repl" && !file_to_stdin(state_.filename)) {
    return;
  }

  if (filename == "repl") {
    restore_stdin();
  }

  try {
    parse();
  } catch (max_warnings_exception& ex) {
    std::cout << "Max warning count exceeded. Aborting." << std::endl;
    logger.finalize();
  } catch (max_errors_exception& ex) {
    std::cout << "Max error count exceeded. Aborting." << std::endl;
    logger.finalize();
  } catch (std::exception& ex) {
    std::cout << "Internal compiler error. Aborting." << std::endl;
    logger.finalize();
  }
}

void Parser::reset_tokenizer() { tokenizer_.reset(); }
size_t Parser::line_number() { return tokenizer_.line_number(); }
size_t Parser::column() { return tokenizer_.column(); }

void Parser::update_tok_position() {
  size_t line_num = tokenizer_.line_number();
  size_t col_num = tokenizer_.column();
  bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
}

int Parser::get_operator_precedence() {
  Token token = tokenizer_.peak();
  if (binop_precedence_.find(token) == binop_precedence_.end()) {
    return -1;
  }

  int precedence = binop_precedence_[token];
  if (precedence <= 0) {
    // undefined operator
    return -1;
  }
  return precedence;
}

std::string Parser::parse_import(std::string current_filename) {
  auto last_line = tokenizer_.line_number();
  auto last_col = tokenizer_.column();
  auto orig_file = current_filename;

  // eat 'import'
  tokenizer_.consume();

  if (tokenizer_.peak() != bon::tok_identifier) {
    bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
    bon::logger.error("syntax error", "expected file name after 'import'");
  }

  std::string filename = tokenizer_.identifier() + ".bon";
  // eat module name
  tokenizer_.consume();

  parse_file(filename);

  bon::logger.set_current_file(orig_file);
  bon::logger.set_line_column(last_line, last_col);

  tokenizer_.reset();

  if (!file_to_stdin(orig_file)) {
    bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
    bon::logger.error("error", "'import' file not found: " + orig_file);
    return current_filename;
  }

  // prime first token
  tokenizer_.consume();

  // fast-forward to previous import line (including eating 'import')
  while (tokenizer_.line_number() <= last_line) {
    tokenizer_.consume();
    if (tokenizer_.peak() == tok_eof) {
      break;
    }
  }
  if (tokenizer_.peak() != tok_eof) {
    // eat file name from previous import
    tokenizer_.consume();
  }

  bon::logger.set_current_file(orig_file);
  bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());

  // needed to reset current file, since the callback can change it
  return current_filename;
}

// literal floating point number
std::unique_ptr<ExprAST> Parser::parse_number_expr() {
  auto expr_node = llvm::make_unique<NumberExprAST>(tokenizer_.line_number(),
                                                    tokenizer_.column(),
                                                    tokenizer_.number_value());
  // eat float
  tokenizer_.consume();
  return std::move(expr_node);
}

// literal integer number
std::unique_ptr<ExprAST> Parser::parse_integer_expr() {
  auto expr_node = llvm::make_unique<IntegerExprAST>(tokenizer_.line_number(),
                                                     tokenizer_.column(),
                                                     tokenizer_.integer_value());
  // eat integer
  tokenizer_.consume();
  return std::move(expr_node);
}

// literal string
std::unique_ptr<ExprAST> Parser::parse_string_expr() {
  auto expr_node = llvm::make_unique<StringExprAST>(tokenizer_.line_number(),
                                                     tokenizer_.column(),
                                                     tokenizer_.identifier());
  // eat string
  tokenizer_.consume();
  return std::move(expr_node);
}

// literal boolean "true" "false"
std::unique_ptr<ExprAST> Parser::parse_bool_expr() {
  auto expr_node = llvm::make_unique<BoolExprAST>(tokenizer_.line_number(),
                                                     tokenizer_.column(),
                                                     tokenizer_.bool_value());
  // eat bool
  tokenizer_.consume();
  return std::move(expr_node);
}

// literal unit type "()"
std::unique_ptr<ExprAST> Parser::parse_unit_expr() {
  auto expr_node = llvm::make_unique<UnitExprAST>(tokenizer_.line_number(),
                                                  tokenizer_.column());
  // eat '()'
  tokenizer_.consume();
  return std::move(expr_node);
}

// '(' expression ')'
std::unique_ptr<ExprAST> Parser::parse_paren_expr() {
  auto line_num = tokenizer_.line_number();
  auto col_num = tokenizer_.column();
  // eat '('
  tokenizer_.consume();
  auto expr_node = parse_expression();
  if (!expr_node)
    return nullptr;

  if (tokenizer_.peak() == tok_comma) {
    // this is a tuple
    std::vector<std::unique_ptr<ExprAST>> tuple_args;
    tuple_args.push_back(std::move(expr_node));
    while (tokenizer_.peak() == tok_comma) {
      // eat ','
      tokenizer_.consume();
      auto expr_node = parse_expression();
      tuple_args.push_back(std::move(expr_node));
    }

    if (tokenizer_.peak() != tok_rparen) {
      bon::logger.set_line_column(line_num, col_num);
      bon::logger.error("syntax error", "expected matching ')'");
      return nullptr;
    }
    // eat ')'
    tokenizer_.consume();
    std::string tuple_name = "Pair";
    if (tuple_args.size() > 2) {
      tuple_name = "Tuple";
      tuple_name += '0' + tuple_args.size();
    }
    // TODO: support heap allocated tuples
    bool heap_alloc = false;
    return llvm::make_unique<ValueConstructorExprAST>(tokenizer_.line_number(),
                                                      tokenizer_.column(),
                                                      tuple_name,
                                                      std::move(tuple_args),
                                                      heap_alloc);
  }

  if (tokenizer_.peak() != tok_rparen) {
    bon::logger.set_line_column(line_num, col_num);
    bon::logger.error("syntax error", "expected matching ')'");
    return nullptr;
  }
  // eat ')'
  tokenizer_.consume();
  return expr_node;
}

// object constructor
// e.g. Some(5)
std::unique_ptr<ExprAST> Parser::parse_value_constructor_expr() {
  bool heap_alloc = false;
  if (tokenizer_.peak() == tok_new) {
    heap_alloc = true;
    // eat new
    tokenizer_.consume();
  }

  std::string constructor_name = tokenizer_.identifier();

  size_t col_num = tokenizer_.column();
  // eat constructor name
  tokenizer_.consume();

  auto line_num = tokenizer_.line_number();

  bool is_enum = tokenizer_.peak() != tok_lparen
              && tokenizer_.peak() != bon::tok_unit;
  bool expecting_args = !is_enum && tokenizer_.peak() != bon::tok_unit;
  if (!is_enum) {
    // eat ( or ()
    tokenizer_.consume();
  }

  // for allowing line breaks
  bool indented = false;
  std::vector<std::unique_ptr<ExprAST>> args;
  if (expecting_args) {
    if (tokenizer_.peak() != tok_rparen) {
      while (true) {
        auto line_num = tokenizer_.line_number();
        if (auto arg = parse_expression()) {
          args.push_back(std::move(arg));
        }
        else {
          return nullptr;
        }

        if (tokenizer_.peak() == tok_rparen) {
          break;
        }

        if (tokenizer_.peak() != tok_comma) {
          bon::logger.set_line_column(line_num, tokenizer_.column());
          bon::logger.error("syntax error",
                            "expected ')' or ',' in argument list");
          // eat bad token
          tokenizer_.consume();
          return nullptr;
        }
        // eat ','
        tokenizer_.consume();
        if (tokenizer_.peak() == tok_indent) {
          if (indented) {
            bon::logger.error("syntax error", "misaligned indentation");
          }
          indented = true;
          // eat tok_indent
          tokenizer_.consume();
          tokenizer_.skip_next_dedent();
        }
      }
    }
  }

  if (expecting_args) {
    // eat ')'
    tokenizer_.consume();
  }

  return llvm::make_unique<ValueConstructorExprAST>(line_num, col_num,
                                                    constructor_name,
                                                    std::move(args),
                                                    heap_alloc);
}

// variable reference, or function call
std::unique_ptr<ExprAST> Parser::parse_identifier_expr() {
  std::string IdName = tokenizer_.identifier();

  size_t col_num = tokenizer_.column();
  auto line_num = tokenizer_.line_number();
  // eat identifier
  tokenizer_.consume();

  // variable reference
  if (tokenizer_.peak() != tok_lparen && tokenizer_.peak() != bon::tok_unit) {
    auto var_expr = llvm::make_unique<VariableExprAST>(line_num, col_num,
                                                       IdName);
    if (vars_in_scope_.count(IdName) > 0) {
      // bon::unify(vars_in_scope_[IdName]->type_var_, var_expr->type_var_);
      delete var_expr->type_var_;
      var_expr->type_var_ = vars_in_scope_[IdName]->type_var_;
    }
    else {
      vars_in_scope_[IdName] = var_expr.get();
      // var_expr->type_var_->get_name();
    }
    return std::move(var_expr);
  }

  // function call
  bool expecting_args = tokenizer_.peak() != bon::tok_unit;
  // eat ( or ()
  tokenizer_.consume();
  std::vector<std::unique_ptr<ExprAST>> args;
  if (expecting_args) {
    if (tokenizer_.peak() != tok_rparen) {
      while (true) {
        auto line_num = tokenizer_.line_number();
        if (auto arg = parse_expression()) {
          args.push_back(std::move(arg));
        }
        else {
          return nullptr;
        }

        if (tokenizer_.peak() == tok_rparen) {
          break;
        }

        if (tokenizer_.peak() != tok_comma) {
          bon::logger.set_line_column(line_num, tokenizer_.column());
          bon::logger.error("syntax error",
                            "expected ')' or ',' in argument list");
          // eat bad token
          tokenizer_.consume();
          return nullptr;
        }
        // eat ','
        tokenizer_.consume();
      }
    }
  }

  line_num = tokenizer_.line_number();
  if (expecting_args) {
    // eat ')'
    tokenizer_.consume();
  }

  auto call_expr = llvm::make_unique<CallExprAST>(line_num, col_num, IdName,
                                                  std::move(args));
  called_functions_.push_back(call_expr.get());
  return call_expr;
}

// sizeof
std::unique_ptr<ExprAST> Parser::parse_sizeof_expr() {
  // eat sizeof
  tokenizer_.consume();

  if (tokenizer_.peak() != tok_lparen) {
    auto line_num = tokenizer_.line_number();
    bon::logger.set_line_column(line_num, tokenizer_.column());
    bon::logger.error("syntax error",
                      "expected '('");
    // eat bad token
    tokenizer_.consume();
    return nullptr;
  }

  // eat (
  tokenizer_.consume();
  auto arg = parse_expression();
  if (tokenizer_.peak() != tok_rparen) {
    auto line_num = tokenizer_.line_number();
    bon::logger.set_line_column(line_num, tokenizer_.column());
    bon::logger.error("syntax error",
                      "expected ')'");
    // eat bad token
    tokenizer_.consume();
    return nullptr;
  }

  // eat ')'
  tokenizer_.consume();

  return llvm::make_unique<SizeofExprAST>(tokenizer_.line_number(),
                                          tokenizer_.column(),
                                          std::move(arg));
}

// ptr_offset
std::unique_ptr<ExprAST> Parser::parse_ptr_offset_expr() {
  auto line_num = tokenizer_.line_number();
  size_t col_num = tokenizer_.column();
  // eat ptr_offset
  tokenizer_.consume();

  if (tokenizer_.peak() != tok_lparen) {
    auto line_num = tokenizer_.line_number();
    bon::logger.set_line_column(line_num, tokenizer_.column());
    bon::logger.error("syntax error",
                      "expected '('");
    // eat bad token
    tokenizer_.consume();
    return nullptr;
  }

  // eat (
  tokenizer_.consume();

  auto arg = parse_expression();

  if (tokenizer_.peak() != tok_comma) {
    bon::logger.set_line_column(line_num, tokenizer_.column());
    bon::logger.error("syntax error",
                      "expected ')' or ',' in argument list");
    // eat bad token
    tokenizer_.consume();
    return nullptr;
  }
  // eat ','
  tokenizer_.consume();

  auto offset = parse_expression();

  if (!arg || !offset) {
    return nullptr;
  }

  if (tokenizer_.peak() != tok_rparen) {
    auto line_num = tokenizer_.line_number();
    bon::logger.set_line_column(line_num, tokenizer_.column());
    bon::logger.error("syntax error",
                      "expected ')'");
    // eat bad token
    tokenizer_.consume();
    return nullptr;
  }

  // eat ')'
  tokenizer_.consume();

  return llvm::make_unique<PtrOffsetExprAST>(tokenizer_.line_number(),
                                             tokenizer_.column(),
                                             std::move(arg),
                                             std::move(offset));
}

std::unique_ptr<ExprAST> Parser::parse_match_expr() {
  auto line_num = tokenizer_.line_number();
  size_t col_num = tokenizer_.column();
  // eat 'match'
  tokenizer_.consume();

  // pattern to match against
  auto pattern = parse_expression();
  if (!pattern) {
    bon::logger.set_line_column(line_num, col_num);
    bon::logger.error("syntax error", "bad match condition");
    return nullptr;
  }

  if (tokenizer_.peak() == bon::tok_indent) {
    // eat expected indentation
    tokenizer_.consume();
  }
  else {
    bon::logger.set_line_column(line_num, col_num);
    bon::logger.error("syntax error", "expected indent after match condition");
    return nullptr;
  }

  std::vector<std::unique_ptr<MatchCaseExprAST>> match_cases;
  auto match_case = parse_expression();
  while (true) {
    if (tokenizer_.peak() != bon::tok_double_arrow) {
      bon::logger.set_line_column(line_num, col_num);
      bon::logger.error("syntax error", "expected '=>' after match case");
      return nullptr;
    }
    // eat '=>'
    tokenizer_.consume();

    bool started_with_indent = false;
    if (tokenizer_.peak() == bon::tok_indent) {
      // eat expected indentation
      tokenizer_.consume();
      started_with_indent = true;
    }

    auto case_body = parse_expression();
    if (!case_body) {
      return nullptr;
    }

    if (started_with_indent) {
      while (tokenizer_.peak() != bon::tok_dedent) {
        // eat optional ';'
        if (tokenizer_.peak() == tok_sep) {
          tokenizer_.consume();
          continue;
        }
        size_t line_num = tokenizer_.line_number();
        size_t col_num = tokenizer_.column();
        auto next_expr = parse_expression();
        if (!next_expr) {
          return nullptr;
        }
        // build expression sequence
        case_body = llvm::make_unique<BinaryExprAST>(line_num,
                                                     col_num, tok_sep,
                                                     std::move(case_body),
                                                     std::move(next_expr));
      }
      // eat unindentation
      tokenizer_.consume();
      if (tokenizer_.peak() != bon::tok_end) {
        bon::logger.set_line_column(line_num, col_num);
        bon::logger.error("syntax error",
                          "expected 'end' after match case block");
        return nullptr;
      }
      // eat 'end'
      tokenizer_.consume();
    }

    match_cases.push_back(llvm::make_unique<MatchCaseExprAST>(
                                                        line_num, col_num,
                                                        std::move(match_case),
                                                        std::move(case_body)));

    // eat optional ';'
    if (tokenizer_.peak() == tok_sep) {
      tokenizer_.consume();
    }

    if (tokenizer_.peak() == bon::tok_dedent) {
      break;
    }
    size_t col_num = tokenizer_.column();
    match_case = parse_expression();
    if (!match_case) {
      return nullptr;
    }
  }
  if (tokenizer_.peak() != bon::tok_dedent) {
    bon::logger.set_line_column(line_num, col_num);
    bon::logger.error("syntax error",
                      "expected unindent after match expression");
    return nullptr;
  }
  // eat unindentation
  tokenizer_.consume();
  if (tokenizer_.peak() != bon::tok_end) {
    bon::logger.set_line_column(line_num, col_num);
    bon::logger.error("syntax error", "expected 'end' after match expression");
    return nullptr;
  }
  // eat 'end'
  tokenizer_.consume();

  return llvm::make_unique<MatchExprAST>(line_num, col_num, std::move(pattern),
                                         std::move(match_cases));
}

/// 'if' expression 'then' expression 'else' expression
std::unique_ptr<ExprAST> Parser::parse_if_expr() {
  auto line_num = tokenizer_.line_number();
  size_t col_num = tokenizer_.column();
  // eat 'if'
  tokenizer_.consume();

  // parse condition
  auto condition_node = parse_expression();
  if (!condition_node) {
    return nullptr;
  }

  // 'then' is optional, unless it's a single-line if expression
  bool started_with_then = false;
  if (tokenizer_.peak() == bon::tok_then) {
    started_with_then = true;
    // eat 'then'
    tokenizer_.consume();
  }

  // track whether this is multi-line,
  //  to determine if 'then' or 'end' are required
  bool started_with_indent = false;
  if (tokenizer_.peak() == bon::tok_indent) {
    // eat expected indentation
    tokenizer_.consume();
    started_with_indent = true;
  }
  else if (!started_with_then) {
    bon::logger.set_line_column(line_num, col_num);
    bon::logger.error("syntax error",
                      "expected 'then' after single-line if condition");
    return nullptr;
  }

  // parse 'then' expression body
  auto then_node = parse_expression();
  if (!then_node) {
    return nullptr;
  }

  if (started_with_indent) {
    while (tokenizer_.peak() != bon::tok_dedent) {
      // eat optional ';'
      if (tokenizer_.peak() == tok_sep) {
        tokenizer_.consume();
        continue;
      }
      size_t line_num = tokenizer_.line_number();
      size_t col_num = tokenizer_.column();
      auto next_expr = parse_expression();
      if (!next_expr) {
        return nullptr;
      }
      // build expression sequence
      then_node = llvm::make_unique<BinaryExprAST>(line_num,
                                                   col_num,
                                                   tok_sep,
                                                   std::move(then_node),
                                                   std::move(next_expr));
    }
    // eat unindentation
    tokenizer_.consume();
  }

  std::unique_ptr<ExprAST> else_node;
  if (tokenizer_.peak() == bon::tok_else) {
    // eat 'else'
    tokenizer_.consume();

    if (started_with_indent) {
      if (tokenizer_.peak() == bon::tok_indent) {
        // eat expected indent
        tokenizer_.consume();
      }
      else {
        bon::logger.set_line_column(tokenizer_.line_number(),
                                    tokenizer_.column());
        bon::logger.error("syntax error",
                          "expected newline with indent after 'else'");
        return nullptr;
      }
    }

    else_node = parse_expression();
    if (!else_node) {
      return nullptr;
    }

    if (started_with_indent) {
      while (tokenizer_.peak() != bon::tok_dedent) {
        // eat optional ';'
        if (tokenizer_.peak() == tok_sep) {
          tokenizer_.consume();
          continue;
        }
        size_t line_num = tokenizer_.line_number();
        size_t col_num = tokenizer_.column();
        auto next_expr = parse_expression();
        if (!next_expr) {
          return nullptr;
        }
        // build expression sequence
        else_node = llvm::make_unique<BinaryExprAST>(line_num,
                                                    col_num,
                                                    tok_sep,
                                                    std::move(else_node),
                                                    std::move(next_expr));
      }
      // eat unindentation
      tokenizer_.consume();
    }
  }

  if (started_with_indent) {
    if (tokenizer_.peak() != bon::tok_end) {
      bon::logger.set_line_column(line_num, col_num);
      bon::logger.error("syntax error", "expected 'end' after if expression");
      return nullptr;
    }
    // eat 'end'
    tokenizer_.consume();
  }
  else {
    if (tokenizer_.peak() == bon::tok_end) {
      // eat optional 'end' in one-line if expression
      tokenizer_.consume();
    }
  }

  return llvm::make_unique<IfExprAST>(line_num, col_num,
                                      std::move(condition_node),
                                      std::move(then_node),
                                      std::move(else_node));
}

// 'while' loop
std::unique_ptr<ExprAST> Parser::parse_while_loop() {
  auto line_num = tokenizer_.line_number();
  size_t col_num = tokenizer_.column();
  // eat 'while'
  tokenizer_.consume();

  // parse condition
  auto condition_node = parse_expression();
  if (!condition_node) {
    return nullptr;
  }

  // 'do' is optional, unless it's a single-line while loop
  bool started_with_do = false;
  if (tokenizer_.peak() == bon::tok_then) {
    started_with_do = true;
    // eat 'then'
    tokenizer_.consume();
  }

  // track whether this is multi-line,
  //  to determine if 'do' or 'end' are required
  bool started_with_indent = false;
  if (tokenizer_.peak() == bon::tok_indent) {
    // eat expected indentation
    tokenizer_.consume();
    started_with_indent = true;
  }
  else if (!started_with_do) {
    bon::logger.set_line_column(line_num, col_num);
    bon::logger.error("syntax error",
                      "expected 'do' after single-line while loop");
    return nullptr;
  }

  // parse 'do' expression body
  auto do_node = parse_expression();
  if (!do_node) {
    return nullptr;
  }

  if (started_with_indent) {
    while (tokenizer_.peak() != bon::tok_dedent) {
      // eat optional ';'
      if (tokenizer_.peak() == tok_sep) {
        tokenizer_.consume();
        continue;
      }
      size_t line_num = tokenizer_.line_number();
      size_t col_num = tokenizer_.column();
      auto next_expr = parse_expression();
      if (!next_expr) {
        return nullptr;
      }
      // build expression sequence
      do_node = llvm::make_unique<BinaryExprAST>(line_num,
                                                   col_num,
                                                   tok_sep,
                                                   std::move(do_node),
                                                   std::move(next_expr));
    }
  }

  if (started_with_indent) {
    // eat unindentation
    tokenizer_.consume();
    if (tokenizer_.peak() != bon::tok_end) {
      bon::logger.set_line_column(line_num, col_num);
      bon::logger.error("syntax error", "expected 'end' after while expression");
      return nullptr;
    }
    // eat 'end'
    tokenizer_.consume();
  }
  else {
    if (tokenizer_.peak() == bon::tok_end) {
      // eat optional 'end' in one-line if expression
      tokenizer_.consume();
    }
  }

  return llvm::make_unique<WhileExprAST>(line_num, col_num,
                                      std::move(condition_node),
                                      std::move(do_node));
}

std::unique_ptr<ExprAST> Parser::parse_list_item(std::unique_ptr<ExprAST> head) {
  auto line_num = tokenizer_.line_number();

  if (tokenizer_.peak() == tok_rbracket) {
    // eat ']'
    tokenizer_.consume();
    std::vector<std::unique_ptr<ExprAST>> empty_args;
    // construct Empty to end list
    std::unique_ptr<ExprAST> empty =
      llvm::make_unique<ValueConstructorExprAST>(tokenizer_.line_number(),
                                                 tokenizer_.column(),
                                                 "Empty",
                                                 std::move(empty_args),
                                                 true);
    // construct Cons
    std::vector<std::unique_ptr<ExprAST>> cons_args;
    cons_args.push_back(std::move(head));
    cons_args.push_back(std::move(empty));
    return llvm::make_unique<ValueConstructorExprAST>(tokenizer_.line_number(),
                                                      tokenizer_.column(),
                                                      "Cons",
                                                      std::move(cons_args),
                                                      true);
  }

  if (tokenizer_.peak() != tok_comma) {
    bon::logger.set_line_column(line_num, tokenizer_.column());
    bon::logger.error("syntax error",
                      "expected ')' or ',' in list constructor");
    // eat bad token
    tokenizer_.consume();
    return nullptr;
  }
  // eat ','
  tokenizer_.consume();

  if (auto item = parse_expression()) {
    auto tail = parse_list_item(std::move(item));
    // construct Cons
    std::vector<std::unique_ptr<ExprAST>> cons_args;
    cons_args.push_back(std::move(head));
    cons_args.push_back(std::move(tail));
    return llvm::make_unique<ValueConstructorExprAST>(tokenizer_.line_number(),
                                                      tokenizer_.column(),
                                                      "Cons",
                                                      std::move(cons_args),
                                                      true);
  }
  else {
    return nullptr;
  }
}

std::unique_ptr<ExprAST> Parser::parse_list_constructor() {
  // eat '['
  tokenizer_.consume();

  auto line_num = tokenizer_.line_number();
  auto col_num = tokenizer_.column();

  auto head = parse_expression();
  if (!head) {
    return nullptr;
  }

  std::vector<std::unique_ptr<ExprAST>> args;
  args.push_back(std::move(head));

  while (tokenizer_.peak() == tok_comma) {
    // eat ','
    tokenizer_.consume();
    auto next = parse_expression();
    if (!next) {
      return nullptr;
    }
    args.push_back(std::move(next));
  }

  if (tokenizer_.peak() != tok_rbracket) {
    bon::logger.set_line_column(line_num, tokenizer_.column());
    bon::logger.error("syntax error",
                      "expected ')' or ',' in list constructor");
    return nullptr;
  }
  // eat ']'
  tokenizer_.consume();

  auto fn_name = "vec" + std::to_string(args.size());
  auto call_expr = llvm::make_unique<CallExprAST>(line_num, col_num, fn_name,
                                                  std::move(args));
  called_functions_.push_back(call_expr.get());
  return call_expr;
}

// parse primary expression
std::unique_ptr<ExprAST> Parser::parse_primary() {
  switch (tokenizer_.peak()) {
    case bon::tok_string:
      return parse_string_expr();
    case bon::tok_true:
    case bon::tok_false:
      return parse_bool_expr();
    case bon::tok_new:
    case bon::tok_typeconstructor:
      return parse_value_constructor_expr();
    case bon::tok_sizeof:
      return parse_sizeof_expr();
    case bon::tok_ptr_offset:
      return parse_ptr_offset_expr();
    case bon::tok_identifier:
      return parse_identifier_expr();
    case bon::tok_number:
      return parse_number_expr();
    case bon::tok_integer:
      return parse_integer_expr();
    case bon::tok_unit:
      return parse_unit_expr();
    case tok_lparen:
      return parse_paren_expr();
    case bon::tok_if:
      return parse_if_expr();
    case bon::tok_while:
      return parse_while_loop();
    case bon::tok_match:
      return parse_match_expr();
    case tok_lbracket:
      return parse_list_constructor();
    default:
      break;
  }

  bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
  std::ostringstream msg;
  auto token = tokenizer_.peak();
  bon::logger.error("syntax error", msg << "unknown token "
                                        << tokenizer_.token_type(token)
                                        << " when expecting an expression");
  return nullptr;
}

bool Parser::is_unary_op(Token op) {
  return tokenizer_.peak() == tok_sub || tokenizer_.peak() == tok_add
         || tokenizer_.peak() == tok_mul;
}

std::unique_ptr<ExprAST> Parser::parse_unary() {
  // if not a supported unary op, must be primary expression
  if (!is_unary_op(tokenizer_.peak()))
    return parse_primary();

  // unary operator expected
  Token unary_op = tokenizer_.peak();
  tokenizer_.consume();
  if (auto Operand = parse_unary()) {
    return llvm::make_unique<UnaryExprAST>(tokenizer_.line_number(),
                                           tokenizer_.column(), unary_op,
                                           std::move(Operand));
  }
  return nullptr;
}

// parse binary operator expression
std::unique_ptr<ExprAST> Parser::parse_binop(int left_precedence,
                                              std::unique_ptr<ExprAST> LHS) {
  // if this is a binop, find its precedence.
  while (true) {
    int this_precedence = get_operator_precedence();

    // if this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (this_precedence < left_precedence) {
      return LHS;
    }

    size_t line_num = tokenizer_.line_number();
    size_t col_num = tokenizer_.column();

    Token BinOp = tokenizer_.peak();
    // eat binop
    tokenizer_.consume();

    if (tokenizer_.peak() == tok_indent) {
      // eat optional indentation
      tokenizer_.consume();
      tokenizer_.skip_next_dedent();
    }

    // Parser::parse_ the primary expression after the binary operator.
    auto RHS = parse_unary();
    if (!RHS) {
      return nullptr;
    }

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = get_operator_precedence();

    // '::', '**', and '++' are all right associative
    if (BinOp == bon::tok_cons
        || BinOp == bon::tok_pow
        || BinOp == bon::tok_concat) {
      --this_precedence;
    }
    if (this_precedence < NextPrec) {
      RHS = parse_binop(this_precedence + 1, std::move(RHS));
      if (!RHS) {
        return nullptr;
      }
    }

    if (BinOp == bon::tok_dot) {
      auto call = dynamic_cast<CallExprAST*>(RHS.get());
      if (call) {
        call->Args.insert(call->Args.begin(), std::move(LHS));
        LHS = std::move(RHS);
        continue;
      }
    }

    // syntactic sugar for infix '::' cons operator
    if (BinOp == bon::tok_cons) {
      std::vector<std::unique_ptr<ExprAST>> args;
      args.push_back(std::move(LHS));
      args.push_back(std::move(RHS));
      LHS = llvm::make_unique<ValueConstructorExprAST>(line_num,
                                                       col_num,
                                                       "Cons",
                                                       std::move(args),
                                                       true);
    }
    else {
      // mark lvalues
      // if (auto lhs_binop = dynamic_cast<BinaryExprAST*>(LHS.get())) {
        if (BinOp == tok_assign) {
          LHS->set_as_lvalue();
        }
      // }
      // merge LHS/RHS.
      LHS = llvm::make_unique<BinaryExprAST>(line_num,
                                             col_num, BinOp,
                                             std::move(LHS), std::move(RHS));
    }
  }
}

// parse all expression types
std::unique_ptr<ExprAST> Parser::parse_expression() {
  auto LHS = parse_unary();
  if (!LHS) {
    return nullptr;
  }

  return parse_binop(0, std::move(LHS));
}

// parse function prototype
std::unique_ptr<PrototypeAST> Parser::parse_prototype() {
  std::string func_name;
  switch(tokenizer_.peak()) {
    case bon::tok_unary:
      tokenizer_.consume();
      if (!is_unary_op(tokenizer_.peak())) {
        bon::logger.set_line_column(tokenizer_.line_number(),
                                    tokenizer_.column());
        bon::logger.error("syntax error",
                          "expected unary operator in prototype");
        return nullptr;
      }
      func_name = "unary";
      func_name += Tokenizer::token_type(tokenizer_.peak());
      tokenizer_.consume();
      break;
    case bon::tok_binary:
      tokenizer_.consume();
      func_name = "operator";
      func_name += Tokenizer::token_type(tokenizer_.peak());
      tokenizer_.consume();
      break;
    case bon::tok_identifier:
      func_name = tokenizer_.identifier();
      tokenizer_.consume();
      break;
    default:
      bon::logger.set_line_column(tokenizer_.line_number(),
                                  tokenizer_.column());
      bon::logger.error("syntax error",
                        "expected function name in prototype");
      return nullptr;
  }

  if (tokenizer_.peak() != tok_lparen && tokenizer_.peak() != bon::tok_unit) {
    bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
    bon::logger.error("syntax error",
                      "expected '(' after function name in prototype");
    return nullptr;
  }

  bool expecting_close_paren = tokenizer_.peak() != bon::tok_unit;

  std::vector<std::string> arg_names;
  std::vector<bool> arg_owned;
  std::vector<bon::TypeVariable*> arg_types;
  // eat '(' or '()'
  tokenizer_.consume();
  while (tokenizer_.peak() == bon::tok_identifier ||
         tokenizer_.peak() == bon::tok_mul) {
    if (tokenizer_.peak() == bon::tok_mul) {
      arg_owned.push_back(true);
      // eat '*'
      tokenizer_.consume();
      if (tokenizer_.peak() != bon::tok_identifier) {
        bon::logger.error("syntax error", "expected arg name after '*'");
      }
    }
    else {
      arg_owned.push_back(false);
    }
    arg_names.push_back(tokenizer_.identifier());
    // eat identifier
    tokenizer_.consume();
    if (tokenizer_.peak() == tok_colon) {
      tokenizer_.consume();
      bon::logger.set_line_column(tokenizer_.line_number(),
                                  tokenizer_.column());
      auto type_var =
            bon::type_variable_from_identifier(tokenizer_.identifier());
      if (type_var == nullptr) {
        // propagate error
        return nullptr;
      }
      arg_types.push_back(type_var);
      // eat type identifier
      tokenizer_.consume();
    }
    else {
      arg_types.push_back(new bon::TypeVariable());
    }
    if (tokenizer_.peak() != tok_comma) {
      break;
    }
    // eat ','
    tokenizer_.consume();
  }

  if (expecting_close_paren) {
    if (tokenizer_.peak() != tok_rparen) {
      bon::logger.set_line_column(tokenizer_.line_number(),
                                  tokenizer_.column());
      bon::logger.error("syntax error", "expected ')' in prototype");
      return nullptr;
    }
    // eat ')'
    tokenizer_.consume();
  }

  bon::TypeVariable* ret_type = nullptr;
  if (tokenizer_.peak() == bon::tok_arrow) {
    // eat '->'
    tokenizer_.consume();
    bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
    ret_type = bon::type_variable_from_identifier(tokenizer_.identifier());
    if (ret_type == nullptr) {
      // propagate error
      return nullptr;
    }
    // eat type identifier
    tokenizer_.consume();
  }
  else {
    ret_type = new bon::TypeVariable();
  }

  auto protoAST = llvm::make_unique<PrototypeAST>(tokenizer_.line_number(),
                                                  tokenizer_.column(),
                                                  func_name,
                                                  std::move(arg_names),
                                                  std::move(arg_types),
                                                  arg_owned,
                                                  ret_type);
  // auto ret_var = get_function_return_type(protoAST->type_var_);
  // bon::unify(ret_type, ret_var);
  return protoAST;
}

std::unique_ptr<TypeAST> Parser::parse_type() {
  size_t line_num = tokenizer_.line_number();
  size_t col_num = tokenizer_.column();
  bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());

  // bon::TypeEnv local_env;
  // bon::push_environment(local_env);
  // bon::AutoScope pop_env([]{ bon::pop_environment(); });

  // eat 'type'
  tokenizer_.consume();

  if (tokenizer_.peak() != bon::tok_identifier) {
    bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
    bon::logger.error("syntax error", "expected type name");
    return nullptr;
  }

  std::string type_name = tokenizer_.identifier();
  tokenizer_.consume();
  auto variant_tvar = new bon::TypeVariable();
  variant_tvar->variant_name_ = type_name;
  // register type before parsing body to allow for recurrent type definitions
  bon::register_type(type_name, variant_tvar);

  std::map<std::string, bon::TypeVariable*> type_parameters;
  if (tokenizer_.peak() == tok_lt) {
    // eat '<'
    tokenizer_.consume();
    while (tokenizer_.peak() != tok_gt) {
      if (tokenizer_.peak() != bon::tok_identifier) {
        bon::logger.set_line_column(tokenizer_.line_number(),
                                    tokenizer_.column());
        bon::logger.error("syntax error", "expected type name");
        return nullptr;
      }
      auto new_tvar = new bon::TypeVariable();
      std::string tparam_name = type_name + ":" + tokenizer_.identifier();
      // std::string tparam_name = "'" + tokenizer_.identifier();
      new_tvar->type_name_ = tparam_name;
      type_parameters[tparam_name] = new_tvar;
      tokenizer_.consume();
      if (tokenizer_.peak() != tok_comma && tokenizer_.peak() != tok_gt) {
        bon::logger.set_line_column(tokenizer_.line_number(),
                                    tokenizer_.column());
        bon::logger.error("syntax error",
                          "expected ',' or '>' in type parameter list");
        return nullptr;
      }
      if (tokenizer_.peak() == tok_comma) {
        // eat ','
        tokenizer_.consume();
      }
    }
    // eat '>'
    tokenizer_.consume();
  }
  else if (tokenizer_.peak() != bon::tok_indent) {
    bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
    bon::logger.error("syntax error", "expected indent after type declaration");
    return nullptr;
  }

  // eat indent
  tokenizer_.consume();

  // parse constructors
  std::map<std::string, bon::TypeVariable*> type_constructors;
  // TODO: fields only supported for non-variant objects
  IndexMap tcon_fields;
  while (tokenizer_.peak() != bon::tok_dedent) {
    if (tokenizer_.peak() != bon::tok_typeconstructor) {
      if (tokenizer_.peak() == bon::tok_identifier) {
        bon::logger.set_line_column(tokenizer_.line_number(),
                                    tokenizer_.column());
        bon::logger.error("syntax error",
                          "expected type constructor "
                          "(first letter must be capitalized)");
        return nullptr;
      }
      bon::logger.set_line_column(tokenizer_.line_number(),
                                  tokenizer_.column());
      bon::logger.error("syntax error", "expected type constructor");
      return nullptr;
    }

    // eat constructor identifier
    std::string tcon_name = tokenizer_.identifier();
    tokenizer_.consume();

    // parse constructor params
    std::vector<bon::TypeVariable*> tcon_params;
    if (tokenizer_.peak() != tok_lparen) {
      // simple enum, no params
      type_constructors[tcon_name] = bon::build_tuple_type(tcon_params);
      continue;
    }
    // eat '('
    tokenizer_.consume();

    // for allowing line breaks
    bool indented = false;
    while (tokenizer_.peak() != tok_rparen) {
      if (tokenizer_.peak() != bon::tok_identifier) {
        bon::logger.set_line_column(tokenizer_.line_number(),
                                    tokenizer_.column());
        bon::logger.error("syntax error", "expected type name");
        return nullptr;
      }
      std::string tparam_name = tokenizer_.identifier();
      // eat identifier
      tokenizer_.consume();
      std::string mangled_tparam_name = type_name + ":" + tparam_name;
      if (type_parameters.find(mangled_tparam_name) != type_parameters.end()) {
        // store reference to variable
        tcon_params.push_back(type_parameters[mangled_tparam_name]);
      }
      else {
        if (tokenizer_.peak() == tok_colon) {
          // eat ':'
          tokenizer_.consume();
          std::string field_name = tparam_name;
          tcon_fields[field_name] = tcon_params.size();
          tparam_name = tokenizer_.identifier();
          // eat identifier
          tokenizer_.consume();
        }
        // must be a concrete type name or type constructor
        auto tvar = bon::type_variable_from_identifier(tparam_name);
        if (!tvar) {
          bon::logger.set_line_column(tokenizer_.line_number(),
                                      tokenizer_.column());
          bon::logger.error("type error", "unknown type referenced");
          // return nullptr;
        }
        tcon_params.push_back(tvar);
      }

      if (tokenizer_.peak() != tok_comma && tokenizer_.peak() != tok_rparen) {
        bon::logger.set_line_column(tokenizer_.line_number(),
                                    tokenizer_.column());
        bon::logger.error("syntax error",
                          "expected ',' or ')' "
                          "in type constructor parameter list");
        return nullptr;
      }
      if (tokenizer_.peak() == tok_comma) {
        // eat ','
        tokenizer_.consume();
      }
      if (tokenizer_.peak() == tok_indent) {
        if (indented) {
          bon::logger.error("syntax error", "misaligned indentation");
        }
        indented = true;
        // eat tok_indent
        tokenizer_.consume();
      }
    }
    // eat ')'
    tokenizer_.consume();
    auto tuple_type = bon::build_tuple_type(tcon_params);
    type_constructors[tcon_name] = tuple_type;
    if (indented) {
      if (tokenizer_.peak() != tok_dedent) {
        bon::logger.error("syntax error", "missing expected unindent");
      }
      else {
        // eat tok_dedent
        tokenizer_.consume();
      }
    }
  }
  // bon::unify(variant_tvar, bon::build_variant_type(type_constructors));
  bon::build_variant_type(variant_tvar, type_constructors, tcon_fields);
  // force building name to make generic
  variant_tvar->get_name();

  // reset type environment
  bon::TypeEnv local_env;
  bon::push_environment(local_env);
  bon::AutoScope pop_env([]{ bon::pop_environment(); });

  if (tokenizer_.peak() != bon::tok_dedent) {
    bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
    bon::logger.error("syntax error", "expected unindent after type body");
    return nullptr;
  }
  // eat unindent
  tokenizer_.consume();

  if (tokenizer_.peak() != bon::tok_end) {
    bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
    bon::logger.error("syntax error", "expected 'end' after type declaration");
    return nullptr;
  }
  // eat 'end'
  tokenizer_.consume();

  return llvm::make_unique<TypeAST>(type_name, line_num, col_num, variant_tvar);
}

// parse function definition
std::unique_ptr<FunctionAST> Parser::parse_definition() {
  size_t line_num = tokenizer_.line_number();
  // eat 'def'
  tokenizer_.consume();
  size_t col_num = tokenizer_.column();
  auto proto_node = parse_prototype();
  if (!proto_node)
    return nullptr;

  bool had_colon = false;
  if (tokenizer_.peak() == tok_colon) {
    had_colon = true;
    // eat ':'
    tokenizer_.consume();
  }

  bool started_with_indent = false;
  if (tokenizer_.peak() == bon::tok_indent) {
    // eat expected indentation
    tokenizer_.consume();
    started_with_indent = true;
  }
  else if (!had_colon) {
    bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());
    bon::logger.error("syntax error",
                      "expected ':' after prototype for single-line function");
    return nullptr;
  }

  bool expecting_return = tokenizer_.peak() == bon::tok_return;
  if (expecting_return) {
    // eat optional 'return'
    tokenizer_.consume();
  }

  // TODO: this should be a stack for nested scoping
  vars_in_scope_.clear();
  called_functions_.clear();

  ExprAST* last_expr = nullptr;
  if (auto E = parse_expression()) {
    last_expr = E.get();
    if (started_with_indent) {
      while (tokenizer_.peak() != bon::tok_dedent) {
        // eat optional ';'
        if (tokenizer_.peak() == tok_sep) {
          tokenizer_.consume();
          continue;
        }
        if (expecting_return) {
          bon::logger.set_line_column(tokenizer_.line_number(),
                                      tokenizer_.column());
          bon::logger.error("syntax error", "expected unindent after 'return'");
          return nullptr;
        }
        bool got_return = tokenizer_.peak() == bon::tok_return;
        if (got_return) {
          // eat optional 'return'
          tokenizer_.consume();
          // function should end after the next expression
          expecting_return = true;
        }
        size_t col_num = tokenizer_.column();
        auto next_expr = parse_expression();
        if (!next_expr) {
          return nullptr;
        }
        last_expr = next_expr.get();
        // build expression sequence
        E = llvm::make_unique<BinaryExprAST>(tokenizer_.line_number(),
                                             col_num, tok_sep,
                                             std::move(E),
                                             std::move(next_expr));
        // if (tokenizer_.peak() != bon::tok_dedent) {
        //   return LogErrorF("Expected unindentation after function body");
        // }
      }
      // eat unindentation
      tokenizer_.consume();
      if (tokenizer_.peak() != bon::tok_end) {
        bon::logger.set_line_column(line_num, col_num);
        bon::logger.error("syntax error", "expected 'end' after function body");
        return nullptr;
      }
      // eat 'end'
      tokenizer_.consume();
    }
    else {
      if (tokenizer_.peak() == bon::tok_end) {
        // eat optional 'end' on single line function
        tokenizer_.consume();
      }
    }

    auto func = llvm::make_unique<FunctionAST>(line_num, col_num,
                                               std::move(proto_node),
                                               std::move(E), last_expr,
                                               called_functions_);
    for (auto arg : func->Proto->Args) {
      // // TODO: Should this just be a warning? Or ignored completely?
      // if (vars_in_scope_.count(arg) == 0) {
      //   std::cout << "Unused parameter " << arg << " in function ",
      //             << func->Proto->Name << std::endl;
      //   return nullptr;
      // }
      auto expr = vars_in_scope_[arg];
      func->Params.push_back(expr);
      if (expr) {
        func->param_name_to_tvar_[arg] = expr->type_var_;
      }
      else {
        func->param_name_to_tvar_[arg] = new bon::TypeVariable();
      }
    }
    vars_in_scope_.clear();

    return func;
  }
  return nullptr;
}

std::unique_ptr<TypeclassAST> Parser::parse_typeclass() {
  size_t line_num = tokenizer_.line_number();
  size_t col_num = tokenizer_.column();
  update_tok_position();

  // eat 'class'
  tokenizer_.consume();

  if (tokenizer_.peak() != tok_typeconstructor) {
    // give a hint about capitalization if they mistakenly
    //  used a lowercase typeclass name
    if (tokenizer_.peak() == tok_identifier) {
      logger.error("syntax error",
                   "expected class name after 'class' (must be capitalized)");
    }
    else {
      logger.error("syntax error", "expected class name after 'class'");
    }
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }

  std::string class_name = tokenizer_.identifier();
  // eat identifier
  tokenizer_.consume();

  if (tokenizer_.peak() != tok_lt) {
    logger.error("syntax error",
                 "expected '<' after 'class " + class_name + "'");
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }

  // eat '<'
  tokenizer_.consume();

  // array of type variable names e.g. ["T"] in class Num<T>
  std::vector<std::string> params;
  // map from type variable name to type variable
  TypeEnv param_types;

  while (tokenizer_.peak() != tok_gt) {
    if (tokenizer_.peak() != tok_typeconstructor) {
      // give a hint about capitalization if they mistakenly
      //  used a lowercase variable name
      if (tokenizer_.peak() == tok_identifier) {
        logger.error("syntax error",
                     "expected type variable (must be capitalized)");
      }
      else {
        logger.error("syntax error", "expected type variable");
        break;
      }
      // eat bad token, but continue to avoid too many errors
      tokenizer_.consume();
    }
    else {
      std::string param_name = tokenizer_.identifier();
      params.push_back(param_name);
      param_types[param_name] = new TypeVariable();

      // eat param name
      tokenizer_.consume();
    }
    if (tokenizer_.peak() != tok_comma && tokenizer_.peak() != tok_gt) {
      logger.error("syntax error", "expected ',' or '>' in type variable list");
      break;
    }

    if (tokenizer_.peak() == tok_comma) {
      tokenizer_.consume();
    }
  }

  // if this isn't true, we must have failed out of the above loop
  if (tokenizer_.peak() == tok_gt) {
    // eat '>'
    tokenizer_.consume();
  }

  if (tokenizer_.peak() == bon::tok_indent) {
    // eat expected indentation
    tokenizer_.consume();
  }
  else {
    update_tok_position();
    bon::logger.error("syntax error", "expected indent after class prototype");
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }

  push_typeclass_environment(param_types);

  TypeMap method_types;
  while (tokenizer_.peak() == tok_def) {
    // eat 'def'
    tokenizer_.consume();
    auto proto = parse_prototype();
    if (tokenizer_.peak() != tok_sep) {
      update_tok_position();
      logger.error("syntax error",
                   "expected ';' after class member declaration");
      // intentionally continue to try to reduce number of errors
      // return nullptr;
      continue;
    }
    TypeVariable* fresh_var = new TypeVariable();
    unify(fresh_var, proto->type_var_);
    method_types[proto->Name] = fresh_var;
    // eat ';'
    tokenizer_.consume();
  }

  param_types = pop_typeclass_environment();

  if (tokenizer_.peak() == bon::tok_dedent) {
    // eat unindentation
    tokenizer_.consume();
  }
  else {
    update_tok_position();
    bon::logger.error("syntax error", "expected unindent after class members");
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }

  if (tokenizer_.peak() != bon::tok_end) {
    update_tok_position();
    bon::logger.error("syntax error", "expected 'end' after class definition");
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }
  // eat 'end'
  tokenizer_.consume();

  return llvm::make_unique<TypeclassAST>(class_name, line_num, col_num,
                                         params, param_types, method_types);;
}

std::unique_ptr<TypeclassImplAST> Parser::parse_typeclass_impl() {
  size_t line_num = tokenizer_.line_number();
  size_t col_num = tokenizer_.column();
  bon::logger.set_line_column(tokenizer_.line_number(), tokenizer_.column());

  // eat 'impl'
  tokenizer_.consume();

  if (tokenizer_.peak() != tok_typeconstructor) {
    // give a hint about capitalization if they mistakenly
    //  used a lowercase typeclass name
    if (tokenizer_.peak() == tok_identifier) {
      logger.error("syntax error",
                   "expected class name after 'impl' (must be capitalized)");
    }
    else {
      logger.error("syntax error", "expected class name after 'impl'");
    }
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }

  std::string class_name = tokenizer_.identifier();
  // eat identifier
  tokenizer_.consume();

  if (tokenizer_.peak() != tok_lt) {
    logger.error("syntax error",
                 "expected '<' after 'impl " + class_name + "'");
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }

  // eat '<'
  tokenizer_.consume();

  // array of type variable names e.g. ["T"] in class Num<T>
  std::vector<std::string> params;
  // map from type variable name to type variable
  TypeEnv param_types;

  while (tokenizer_.peak() != tok_gt) {
    if (tokenizer_.peak() != tok_identifier) {
      // give a hint about capitalization if they mistakenly
      //  used a generic type variable name
      if (tokenizer_.peak() == tok_typeconstructor) {
        logger.error("syntax error",
                     "expected concrete type (got a type variable instead)");
      }
      else {
        logger.error("syntax error", "expected concrete type name");
      }
      // eat bad token, but continue to avoid too many errors
      tokenizer_.consume();
    }
    else {
      std::string param_name = tokenizer_.identifier();
      params.push_back(param_name);
      param_types[param_name] = new TypeVariable();

      // eat type name
      tokenizer_.consume();
    }
    if (tokenizer_.peak() != tok_comma && tokenizer_.peak() != tok_gt) {
      logger.error("syntax error", "expected ',' or '>' in type list");
    }

    if (tokenizer_.peak() == tok_comma) {
      tokenizer_.consume();
    }
  }

  // if this isn't true, we must have failed out of the above loop
  if (tokenizer_.peak() == tok_gt) {
    // eat '>'
    tokenizer_.consume();
  }

  if (tokenizer_.peak() == bon::tok_indent) {
    // eat expected indentation
    tokenizer_.consume();
  }
  else {
    update_tok_position();
    bon::logger.error("syntax error", "expected indent after class prototype");
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }

  std::map<std::string, std::unique_ptr<FunctionAST>> funcs;
  while (tokenizer_.peak() == tok_def) {
    auto func = parse_definition();
    if (func) {
      state_.ordered_functions.push_back(func.get());
      funcs[func->Proto->Name] = std::move(func);
    }
  }

  if (tokenizer_.peak() == bon::tok_dedent) {
    // eat unindentation
    tokenizer_.consume();
  }
  else {
    update_tok_position();
    bon::logger.error("syntax error", "expected unindent after class members");
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }

  if (tokenizer_.peak() != bon::tok_end) {
    update_tok_position();
    bon::logger.error("syntax error", "expected 'end' after class impl");
    // intentionally continue to try to reduce number of errors
    // return nullptr;
  }
  // eat 'end'
  tokenizer_.consume();
  TypeMap emptymap;
  return llvm::make_unique<TypeclassImplAST>(class_name, line_num, col_num,
                                             param_types, emptymap,
                                             std::move(funcs));
}

// parse top-level expression
std::unique_ptr<FunctionAST> Parser::parse_toplevel_expr() {
  called_functions_.clear();
  if (auto E = parse_expression()) {
    // Make an anonymous proto.
    auto proto_node = llvm::make_unique<PrototypeAST>(
                                              tokenizer_.line_number(),
                                              tokenizer_.column(),
                                              "top-level",
                                              std::vector<std::string>(),
                                              std::vector<bon::TypeVariable*>(),
                                              std::vector<bool>(),
                                              new bon::TypeVariable());
    // bon::unify(proto_node->ret_type_,
    //            get_function_return_type(proto_node->type_var_));
    // bon::unify(proto_node->ret_type_, bon::UnitType);
    return llvm::make_unique<FunctionAST>(tokenizer_.line_number(),
                                          tokenizer_.column(),
                                          std::move(proto_node),
                                          std::move(E),
                                          E.get(),
                                          called_functions_);
  }
  return nullptr;
}

// 'cdef' prototype
std::unique_ptr<PrototypeAST> Parser::parse_extern() {
  // eat 'cdef'
  tokenizer_.consume();
  auto proto_node = parse_prototype();
  bon::unify(proto_node->ret_type_,
             get_function_return_type(proto_node->type_var_));
  return proto_node;
}

} // namespace bon
