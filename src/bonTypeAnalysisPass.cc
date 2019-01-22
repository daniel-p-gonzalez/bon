/*----------------------------------------------------------------------------*\
|*
|* Type Analysis AST walker which performs type inference and checking
|*
L*----------------------------------------------------------------------------*/

#include "bonTypeAnalysisPass.h"
#include "bonLogger.h"
#include "auto_scope.h"

namespace bon {

// floating point number
void TypeAnalysisPass::process(NumberExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
}

// integer number
void TypeAnalysisPass::process(IntegerExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
}

// string
void TypeAnalysisPass::process(StringExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
}

// boolean
void TypeAnalysisPass::process(BoolExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
}

// unit ()
void TypeAnalysisPass::process(UnitExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
}

// variable
void TypeAnalysisPass::process(VariableExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
}

// value constructor
void TypeAnalysisPass::process(ValueConstructorExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  TypeVariable* variant_type = get_type_from_constructor(node->constructor_);

  node->push_type_environment();
  std::vector<TypeVariable*> param_types;
  for (auto &arg : node->tcon_args_) {
    arg->run_pass(this);
    param_types.push_back(arg->type_var_);
  }
  auto tuple_type = build_tuple_type(param_types);
  param_types.clear();
  if (tuple_type != nullptr) {
    param_types.push_back(tuple_type);
  }
  auto tcon_var = build_from_type_constructor(node->constructor_, param_types);
  // unify constructor with variant type
  unify(tcon_var, variant_type);
  unify(node->type_var_, tcon_var);
  auto val_type = flatten_variable(variant_type);
  node->type_env_ = pop_environment();
}

// unary op
void TypeAnalysisPass::process(UnaryExprAST* node) {
  push_environment(node->Env);
  AutoScope pop_env([this, node]{
      node->Env = pop_environment();
      std::string mangled_name = node->get_mangled_name();
      auto Callee = std::string("unary") + Tokenizer::token_type(node->Opcode);
      state_.function_envs[Callee].push_back(std::make_pair(mangled_name,
                                                            node->Env));
    });
  logger.set_line_column(node->line_num_, node->column_num_);
  node->Operand->run_pass(this);
  unify(node->Operand->type_var_, node->type_var_);
}

// BinaryExprAST
void TypeAnalysisPass::process(BinaryExprAST* node) {
  static std::set<Token> ops = {tok_add, tok_sub, tok_mul, tok_div,
                                tok_assign, tok_sep, tok_cons, tok_dot};
  if (ops.count(node->Op) == 0) {
    push_environment(node->Env);
    AutoScope pop_env([this, node]{
        node->Env = pop_environment();
        auto Callee = std::string("operator") + Tokenizer::token_type(node->Op);
        std::stringstream mangled_name_stream;
        mangled_name_stream << state_.filename << ":" << node->line_num_
                            << ":" << Callee << ":"
                            << static_cast<const void*>(this);
        std::string mangled_name = mangled_name_stream.str();
        state_.function_envs[Callee].push_back(std::make_pair(mangled_name,
                                                              node->Env));
      });
  }

  node->LHS->run_pass(this);
  node->RHS->run_pass(this);
  logger.set_line_column(node->line_num_, node->column_num_);
  if (node->inherit_child_type_) {
    unify(node->type_var_, node->RHS->type_var_);
  }
  // don't attempt to unify neighboring expressions in sequence
  if (node->Op != tok_sep && node->Op != tok_cons
                          && node->Op != tok_dot) {
    unify(node->LHS->type_var_, node->RHS->type_var_);
  }
}

// IfExprAST
void TypeAnalysisPass::process(IfExprAST* node) {
  node->Cond->run_pass(this);
  node->Then->run_pass(this);
  node->Else->run_pass(this);
  logger.set_line_column(node->line_num_, node->column_num_);
  unify(node->Cond->type_var_, BoolType);
  unify(node->Then->type_var_, node->Else->type_var_);
  unify(node->type_var_, node->Then->type_var_);
  // return flatten_variable(node->type_var_);
}

// MatchCaseExprAST
void TypeAnalysisPass::process(MatchCaseExprAST* node) {
  node->condition_->run_pass(this);
  node->body_->run_pass(this);
  unify(node->body_->type_var_, node->type_var_);
}

static TypeVariable* get_parent_type(TypeVariable* child) {
  child = resolve_variable(child);
  if (child->type_operator_ &&
      child->type_operator_->type_constructor_ != " | ") {
    auto variant_type =
          get_type_from_constructor(child->type_operator_->type_constructor_);
    if (variant_type) {
      return resolve_variable(variant_type);
    }
  }
  return child;
}

// MatchExprAST
void TypeAnalysisPass::process(MatchExprAST* node) {
  node->pattern_->run_pass(this);
  TypeVariable* body_var = nullptr;
  for (auto &match_case : node->match_cases_) {
    match_case->run_pass(this);
    unify(get_parent_type(match_case->condition_->type_var_),
          get_parent_type(node->pattern_->type_var_));
    if (body_var) {
      unify(get_parent_type(body_var),
            get_parent_type(match_case->body_->type_var_));
    }
    body_var = get_parent_type(match_case->body_->type_var_);
  }
  unify(get_parent_type(body_var), get_parent_type(node->type_var_));
}

// CallExprAST
void TypeAnalysisPass::process(CallExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);

  push_environment(node->Env);
  AutoScope pop_env([this, node]{
      node->Env = pop_environment();
      std::string mangled_name = node->get_mangled_name();
      state_.function_envs[node->Callee].push_back(std::make_pair(mangled_name,
                                                                  node->Env));
    });

  std::vector<TypeVariable*> arg_types;
  for (auto &arg : node->Args) {
    arg->run_pass(this);
    arg_types.push_back(arg->type_var_);
  }

  TypeVariable* func_type_var = build_function_type(arg_types);

  if (state_.method_to_typeclass.find(node->Callee) !=
      state_.method_to_typeclass.end()) {
    if (!is_concrete_type(func_type_var->type_operator_->types_[0])) {
      std::string typeclass_name = state_.method_to_typeclass[node->Callee];
      auto &typeclass = state_.typeclasses[typeclass_name];
      unify(func_type_var, typeclass->methods_[node->Callee]);
      auto ret_var = get_function_return_type(func_type_var);
      unify(node->type_var_, ret_var);
      return;
      // return flatten_variable(node->type_var_);
    }
  }

  auto func = state_.all_functions.count(node->Callee) > 0 ?
              state_.all_functions[node->Callee].get()
              :
              state_.get_typeclass_impl_function_node(node->Callee,
                                                      func_type_var);

  // reset line/column changed by arguments
  logger.set_line_column(node->line_num_, node->column_num_);

  if (!func && state_.all_functions.count(node->Callee) == 0) {
    // TODO: handle this by creating an ExternFunctionAST node
    //       with a type_var_ we can unify with
    // skip extern functions
    auto FI = state_.function_protos.find(node->Callee);
    if (FI != state_.function_protos.end()) {
      auto &protoAST = FI->second;
      if (node->Args.size() != protoAST->Args.size()) {
        std::ostringstream msg;
        logger.error("error", msg << "function " << node->Callee <<
                     " takes " << protoAST->Args.size() << " argument(s), but "
                     << node->Args.size() << " were given");
        return;
      }
      unify(func_type_var, protoAST->type_var_);
      auto ret_var = get_function_return_type(protoAST->type_var_);
      unify(node->type_var_, ret_var);
      // return flatten_variable(node->type_var_);
      return;
    }
    std::ostringstream msg;
    logger.error("error", msg << "calling undefined function " << node->Callee);
    return;
  }

  if (func == nullptr) {
    std::ostringstream msg;
    logger.error("internal error", msg << "function "
                                       << node->Callee
                                       << " missing from active node list");
    return;
  }

  if (node->Args.size() != func->Params.size()) {
    std::ostringstream msg;
    logger.error("error", msg << "function " << node->Callee << " takes "
                              << func->Params.size() << " argument(s), but "
                              << node->Args.size() << " were given");
    return;
  }

  get_fresh_variable(func->type_var());

  unify(func_type_var, func->type_var());
  unify(node->type_var_, func->Body->type_var_);
  // return flatten_variable(node->type_var_);
}

// PrototypeAST
void TypeAnalysisPass::process(PrototypeAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  auto ret_var = get_function_return_type(node->type_var_);
  unify(node->ret_type_, ret_var);
}

// FunctionAST
void TypeAnalysisPass::process(FunctionAST* node) {
  node->Proto->run_pass(this);
  logger.set_line_column(node->line_num_, node->column_num_);
  std::vector<TypeVariable*> param_types;
  for (auto param : node->Params) {
    if (!param) {
      continue;
    }
    // param->run_pass(this);
    param_types.push_back(param->type_var_);
  }
  auto func_type_var = build_function_type(param_types);
  unify(node->Proto->type_var_, func_type_var);
  // TODO: clean this up
  // unify return type with body expression type
  auto ret_type = get_function_return_type(node->type_var());
  unify(node->last_expr_->type_var_, ret_type);
  unify(node->Proto->ret_type_, ret_type);
  node->Body->run_pass(this);
  unify(node->Body->type_var_, ret_type);
  unify(node->Body->type_var_, ret_type);
  // force generating names for free variables
  node->type_var()->get_name();
  // Proto->type_var_->get_name();
  // Proto->ret_type_->get_name();
}

// TypeAST
void TypeAnalysisPass::process(TypeAST* node) {
}

// TypeclassAST
void TypeAnalysisPass::process(TypeclassAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  for (auto &impl : node->impls) {
    impl->run_pass(this);
  }
}

// TypeclassImplAST
void TypeAnalysisPass::process(TypeclassImplAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  for (auto &method_entry : node->methods_) {
    auto &method = method_entry.second;
    method->run_pass(this);
  }
}

} // namespace bon
