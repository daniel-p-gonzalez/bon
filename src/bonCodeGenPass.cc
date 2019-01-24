/*----------------------------------------------------------------------------*\
|*
|* Code Gen AST walker for generating machine code using LLVM
|*
L*----------------------------------------------------------------------------*/
#include "bonCodeGenPass.h"
#include "bonAST.h"
#include "bonLogger.h"
#include "bonLLVM.h"
#include "auto_scope.h"

namespace bon {

// sentinel value for non-branching paths
// TODO: this is so unsafe it's not even funny
Value* s_NoBranch = (Value*)1957;

std::unique_ptr<PrototypeAST> copy_proto(PrototypeAST &proto,
                                         std::string name) {
  auto new_proto = llvm::make_unique<PrototypeAST>(proto.line_num_,
                                                   proto.column_num_, name,
                                                   proto.Args,
                                                   std::vector<TypeVariable*>(),
                                                   proto.ret_type_);
  new_proto->type_var_ = proto.type_var_;
  return new_proto;
}

/*----------------------------------------------------------------------------*\
|* CodeGenPass
L*----------------------------------------------------------------------------*/

Function* CodeGenPass::get_function(std::string name) {
  auto function = state_.get_function(name);
  if (function) {
    return function;
  }

  // check existing function prototype
  auto func_proto = state_.function_protos.find(name);
  if (func_proto != state_.function_protos.end()) {
    func_proto->second->run_pass(this);
    return (Function*)result();
  }

  return nullptr;
}

// NumberExprAST
void CodeGenPass::process(NumberExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  returns (ConstantFP::get(state_.llvm_context, APFloat(node->Val)));
}

// IntegerExprAST
void CodeGenPass::process(IntegerExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  returns (ConstantInt::get(state_.llvm_context, APInt(64, node->Val, true)));
}

// StringExprAST
void CodeGenPass::process(StringExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  returns (state_.builder.CreateGlobalStringPtr(node->Val.c_str()));
}

// BoolExprAST
void CodeGenPass::process(BoolExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  auto val = node->Val ? 1 : 0;
  returns (ConstantInt::get(state_.llvm_context,
           APInt(/*nbits*/1, val, /*is_signed*/false)));
}

// UnitExprAST
void CodeGenPass::process(UnitExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);

  auto tname = node->type_var_->get_name();
  StructType* structReg = state_.struct_map[tname];
  if (!structReg) {
    std::vector<Type*> members;
    std::vector<Value*> member_values;
    members.push_back(Type::getInt32Ty(state_.llvm_context));
    structReg = StructType::create(state_.llvm_context, members,
                                   "struct." + tname);
    state_.struct_map[tname] = structReg;
  }
  auto entry_block = state_.builder.GetInsertBlock();

  Value* val_alloc = ConstantExpr::getNullValue(structReg);

  Value* val_type_ptr =
      state_.builder.CreateStructGEP(structReg, val_alloc, 0,
                                     val_alloc->getName() + ".at(0)");

  auto tcon_enum = 0;
  auto tcon_val = ConstantInt::get(state_.llvm_context,
                                   APInt(/*nbits*/32, tcon_enum,
                                         /*is_signed*/false));
  auto result = state_.builder.CreateStore(tcon_val, val_type_ptr);

  returns (state_.builder.CreateLoad(val_alloc, "()"));
}

// VariableExprAST
void CodeGenPass::process(VariableExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  // Look this variable up in the function.
  Value* var_value = state_.named_values[node->Name];
  if (!var_value)
  {
    std::ostringstream msg;
    logger.error("syntax error", msg << "variable " << node->Name
                                     << " is not defined in this scope");
    returns (nullptr);
    return;
  }

  returns (state_.builder.CreateLoad(var_value, node->Name.c_str()));
}

// ValueConstructorExprAST
void CodeGenPass::process(ValueConstructorExprAST* node) {
  node->push_type_environment();

  std::vector<Type*> members;
  std::vector<Value*> member_values;
  members.push_back(Type::getInt32Ty(state_.llvm_context));
  for (auto &arg : node->tcon_args_) {
    try {
      members.push_back(get_value_type_dispatch(arg.get()));
      arg->run_pass(this);
      auto arg_result = result();
      member_values.push_back(arg_result);
    } catch(std::exception &ex) {
      std::cout << node->constructor_ << " failed with arg "
                << arg->type_var_->get_name() << std::endl;
      std::cout << ex.what() << std::endl;
    }
  }

  auto tname = node->type_var_->get_name();
  StructType* structReg = state_.struct_map[tname];
  if (true) {//!structReg) {
    structReg = StructType::create(state_.llvm_context, members,
                                   "struct." + tname);
    state_.struct_map[tname] = structReg;
  }

  assert(!structReg->isPointerTy());
  auto structRegPtr = PointerType::get(structReg, 0);
  // structReg->dump();

  node->pop_type_environment();

  auto entry_block = state_.builder.GetInsertBlock();

  Type* ITy = Type::getInt32Ty(state_.llvm_context);
  Constant* AllocSize = ConstantExpr::getSizeOf(structReg);
  AllocSize = ConstantExpr::getTruncOrBitCast(AllocSize, ITy);
  Instruction* val_alloc = CallInst::CreateMalloc(entry_block,
                                                  ITy, structReg, AllocSize,
                                                  nullptr, nullptr);

  MDNode* metadata = MDNode::get(state_.llvm_context,
                                 MDString::get(state_.llvm_context,
                                 logger.get_context()));
  val_alloc->setMetadata("context", metadata);

  state_.builder.Insert(val_alloc, node->constructor_);

  Value* ArgValuePtr =
        state_.builder.CreateStructGEP(structReg, val_alloc, 0,
                                       val_alloc->getName() + ".at(0)");

  auto tcon_enum = get_constructor_value(node->constructor_);
  auto tcon_val = ConstantInt::get(state_.llvm_context,
                                   APInt(/*nbits*/32, tcon_enum,
                                         /*is_signed*/false));
  auto result = state_.builder.CreateStore(tcon_val, ArgValuePtr);
  size_t offset = 1;
  for (auto &val : member_values) {
    // structReg->dump();
    auto val_name = val_alloc->getName();
    auto val_ptr =
      state_.builder.CreateStructGEP(structReg, val_alloc, offset,
                                     val_name + ".at("
                                              + std::to_string(offset)
                                              + ")");
    // trust type checker and cast to generic pointer type for e.g. variant args
    auto val_bitcast =
      state_.builder.CreateBitOrPointerCast(
                                        val,
                                        structReg->getStructElementType(offset),
                                        val->getName() + ".bitcast");
    state_.builder.CreateStore(val_bitcast, val_ptr);
    ++offset;
  }

  returns (val_alloc);
}

// UnaryExprAST
void CodeGenPass::process(UnaryExprAST* node) {
  node->Operand->run_pass(this);
  Value* operand_value = result();
  logger.set_line_column(node->line_num_, node->column_num_);
  if (!operand_value) {
    returns (nullptr);
    return;
  }

  auto callee = std::string("unary") + Tokenizer::token_type(node->Opcode);

  std::vector<TypeVariable*> arg_types;
  arg_types.push_back(node->Operand->type_var_);

  TypeVariable* func_type_var = build_function_type(arg_types);

  if (state_.method_to_typeclass.find(callee) !=
      state_.method_to_typeclass.end()) {
    if (!is_concrete_type(node->Operand->type_var_)) {
      logger.error("error",
                   "unable to infer type of operand for unary operator "
                   + callee);
      returns (nullptr);
      return;
    }
  }

  auto func = state_.all_functions.count(callee) > 0 ?
              state_.all_functions[callee].get()
              :
              state_.get_typeclass_impl_function_node(callee,
                                                      func_type_var);
  if (func == nullptr) {
    std::ostringstream msg;
    logger.error("syntax error", msg << "undefined unary operator "
                                     << node->Opcode);
    returns (nullptr);
    return;
  }
  else {
    func_type_var = func->type_var();
  }

  std::stringstream type_name;
  type_name << callee << " = " << func_type_var->get_name();
  std::string mangled_name = type_name.str();
  Function* F = state_.get_typeclass_impl_function(callee, mangled_name);
  if (!F) {
    F = get_function(mangled_name);
    if (!F) {
      std::ostringstream msg;
      logger.error("syntax error", msg << "undefined unary operator "
                                       << node->Opcode);
      returns (nullptr);
      return;
    }
  }

  returns (state_.builder.CreateCall(F, operand_value, "unaryop"));
}

// BinaryExprAST
void CodeGenPass::process(BinaryExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  Value* l_value = nullptr;
  if (node->Op != tok_assign) {
    node->LHS->run_pass(this);
    l_value = result();
  }
  Value* r_value = nullptr;
  if (node->Op != tok_dot) {
    node->RHS->run_pass(this);
    r_value = result();
  }

  if (!l_value || !r_value) {
    if (node->Op != tok_assign || !r_value) {
      if (node->Op != tok_dot || !l_value) {
        returns (nullptr);
        return;
      }
    }
  }

  Value* variable = nullptr;
  Function* function = nullptr;
  AllocaInst* Alloca = nullptr;

  switch (node->Op) {
  case tok_assign:
    // Look up the name.
    variable = state_.named_values[node->LHS->getName()];
    if (!variable) {
      function = state_.builder.GetInsertBlock()->getParent();
      // Create an alloca for this variable.
      node->RHS->push_type_environment();
      Alloca = create_entry_block_alloca(function, node->LHS->getName(),
                                         node->RHS->type_var_,
                                         node->RHS.get());
      node->RHS->pop_type_environment();

      // Add arguments to variable symbol table.
      state_.named_values[node->LHS->getName()] = Alloca;
      variable = Alloca;
    }

    // Store the initial value into the alloca.
    state_.builder.CreateStore(r_value, variable);
    // named_values_[LHS->getName()] = r_value;
    returns (r_value);
    return;
  // sequencing operator
  case tok_sep:
    returns (r_value);
    return;
  case tok_dot:
    {
      std::string constructor = get_constructor_from_type(node->LHS->type_var_);
      std::string field = node->RHS->getName();
      auto field_index = get_constructor_field_index(constructor, field);
      auto tname = node->LHS->type_var_->get_name(false);
      StructType* structReg = state_.struct_map[tname];
      if (!structReg) {
        bon::logger.error("internal error",
                          "struct type not found for type: " + tname);
      }
      // get constants for our indices
      auto el_idx0 = ConstantInt::get(state_.llvm_context,
                                      APInt(/*nbits*/32, 0, /*is_signed*/false));
      auto el_idx1 = ConstantInt::get(state_.llvm_context,
                                      APInt(/*nbits*/32,
                                            field_index+1,
                                            /*is_signed*/false));

      // first index (0) dereferences pointer to struct,
      // second index (field_index+1) points at constructor field
      //  (e.g. x in Point2D(x:int, y:int))
      std::vector<Value*> indices;
      indices.push_back(el_idx0);
      indices.push_back(el_idx1);

      // grab pointer to constructor index for variant we're matching against
      Value* patt_type_ptr = state_.builder.CreateGEP(structReg, l_value,
                                                      indices);
      // load constructor index for match input
      auto patt_type_val = state_.builder.CreateLoad(patt_type_ptr);

      // auto val_ptr =
      //   state_.builder.CreateStructGEP(structReg, l_value, field_index+1,
      //                                  field);
      returns (patt_type_val);
    }
    return;
  case tok_add:
    if (resolve_variable(node->LHS->type_var_) == FloatType) {
      returns (state_.builder.CreateFAdd(l_value, r_value, "addtmp"));
      return;
    }
    else if (resolve_variable(node->LHS->type_var_) == IntType) {
      returns (state_.builder.CreateAdd(l_value, r_value, "addtmp"));
      return;
    }
    else {
      std::ostringstream msg;
      logger.error("codegen error", msg << "operator + not defined for type "
                                        << node->LHS->type_var_->get_name());
      returns (nullptr);
      return;
    }
  case tok_sub:
    if (resolve_variable(node->LHS->type_var_) == FloatType) {
      returns (state_.builder.CreateFSub(l_value, r_value, "subtmp"));
      return;
    }
    else if (resolve_variable(node->LHS->type_var_) == IntType) {
      returns (state_.builder.CreateSub(l_value, r_value, "subtmp"));
      return;
    }
  case tok_mul:
    if (resolve_variable(node->LHS->type_var_) == FloatType) {
      returns (state_.builder.CreateFMul(l_value, r_value, "multmp"));
      return;
    }
    else if (resolve_variable(node->LHS->type_var_) == IntType) {
      returns (state_.builder.CreateMul(l_value, r_value, "multmp"));
      return;
    }
  case tok_div:
    if (resolve_variable(node->LHS->type_var_) == FloatType) {
      returns (state_.builder.CreateFDiv(l_value, r_value, "divtmp"));
      return;
    }
    else if (resolve_variable(node->LHS->type_var_) == IntType) {
      returns (state_.builder.CreateSDiv(l_value, r_value, "divtmp"));
      return;
    }
  case tok_gt:
    if (resolve_variable(node->LHS->type_var_) == FloatType) {
      returns (state_.builder.CreateFCmpUGT(l_value, r_value, "cmptmp"));
      return;
    }
    else if (resolve_variable(node->LHS->type_var_) == IntType) {
      returns (state_.builder.CreateICmpSGT(l_value, r_value, "cmptmp"));
      return;
    }
  case tok_lt:
    if (resolve_variable(node->LHS->type_var_) == FloatType) {
      returns (state_.builder.CreateFCmpULT(l_value, r_value, "cmptmp"));
      return;
    }
    else if (resolve_variable(node->LHS->type_var_) == IntType) {
      returns (state_.builder.CreateICmpSLT(l_value, r_value, "cmptmp"));
      return;
    }
  case tok_gteq:
    if (resolve_variable(node->LHS->type_var_) == FloatType) {
      returns (state_.builder.CreateFCmpUGE(l_value, r_value, "cmptmp"));
      return;
    }
    else if (resolve_variable(node->LHS->type_var_) == IntType) {
      returns (state_.builder.CreateICmpSGE(l_value, r_value, "cmptmp"));
      return;
    }
  case tok_lteq:
    if (resolve_variable(node->LHS->type_var_) == FloatType) {
      returns (state_.builder.CreateFCmpULE(l_value, r_value, "cmptmp"));
      return;
    }
    else if (resolve_variable(node->LHS->type_var_) == IntType) {
      returns (state_.builder.CreateICmpSLE(l_value, r_value, "cmptmp"));
      return;
    }
  case tok_eq:
    if (resolve_variable(node->LHS->type_var_) == FloatType) {
      returns (state_.builder.CreateFCmpUEQ(l_value, r_value, "cmptmp"));
      return;
    }
    else if (resolve_variable(node->LHS->type_var_) == IntType) {
      returns (state_.builder.CreateICmpEQ(l_value, r_value, "cmptmp"));
      return;
    }
  default:
    break;
  }

  auto Callee = std::string("operator") + Tokenizer::token_type(node->Op);

  std::vector<TypeVariable*> arg_types;
  arg_types.push_back(node->LHS->type_var_);
  arg_types.push_back(node->RHS->type_var_);

  TypeVariable* func_type_var = build_function_type(arg_types);

  if (state_.method_to_typeclass.find(Callee) !=
      state_.method_to_typeclass.end()) {
    if (!is_concrete_type(node->LHS->type_var_) ||
        !is_concrete_type(node->RHS->type_var_)) {
      logger.error("error",
                   "unable to infer type of operand for binary operator "
                   + Callee);
      returns (nullptr);
      return;
    }
  }

  auto function_node = state_.all_functions.count(Callee) > 0 ?
                       state_.all_functions[Callee].get()
                       :
                       state_.get_typeclass_impl_function_node(Callee,
                                                               func_type_var);
  if (function_node == nullptr) {
    std::ostringstream msg;
    logger.error("syntax error", msg << "undefined binary operator "
                                     << Tokenizer::token_type(node->Op));
    returns (nullptr);
    return;
  }
  else {
    func_type_var = function_node->type_var();
  }

  std::stringstream type_name;
  type_name << Callee << " = " << func_type_var->get_name();
  std::string mangled_name = type_name.str();
  Function* F = state_.get_typeclass_impl_function(Callee, mangled_name);
  if (!F) {
    F = get_function(mangled_name);
    if (!F) {
      std::ostringstream msg;
      logger.error("syntax error", msg << "undefined binary operator "
                                       << Tokenizer::token_type(node->Op));
      returns (nullptr);
      return;
    }
  }

  std::vector<Value*> call_args;
  call_args.push_back(l_value);
  call_args.push_back(r_value);

  returns (state_.builder.CreateCall(F, call_args, "binop"));
}

// IfExprAST
void CodeGenPass::process(IfExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  node->Cond->run_pass(this);
  Value* CondV = result();
  if (!CondV) {
    returns (nullptr);
    return;
  }

  Function* function = state_.builder.GetInsertBlock()->getParent();

  // Create blocks for the then and else cases.  Insert the 'then' block at the
  // end of the function.
  BasicBlock* then_block = BasicBlock::Create(state_.llvm_context, "then",
                                              function);
  BasicBlock* else_block = BasicBlock::Create(state_.llvm_context, "else");
  BasicBlock* merge_block = BasicBlock::Create(state_.llvm_context, "ifmerge");

  state_.builder.CreateCondBr(CondV, then_block, else_block);

  // begin gen for 'then' block
  state_.builder.SetInsertPoint(then_block);

  node->Then->run_pass(this);
  Value* then_value = result();
  if (!then_value) {
    returns (nullptr);
    return;
  }

  state_.builder.CreateBr(merge_block);
  // Codegen of 'then' can change the current block, so update then_block
  //  for the PHI merge below
  then_block = state_.builder.GetInsertBlock();

  // begin gen for 'else' block
  function->getBasicBlockList().push_back(else_block);
  state_.builder.SetInsertPoint(else_block);

  node->Else->run_pass(this);
  Value* else_value = result();
  if (!else_value) {
    returns (nullptr);
    return;
  }

  state_.builder.CreateBr(merge_block);
  // codegen of 'else' body can change the current block,
  //  so update else_block for the PHI merge below
  else_block = state_.builder.GetInsertBlock();

  // begin gen for merge block
  function->getBasicBlockList().push_back(merge_block);
  state_.builder.SetInsertPoint(merge_block);
  PHINode* phi_node =
    state_.builder.CreatePHI(get_value_type_dispatch(node->Then.get()),
                             2, "ifPhi");

  phi_node->addIncoming(then_value, then_block);
  phi_node->addIncoming(else_value, else_block);
  returns (phi_node);
}

// MatchCaseExprAST
void CodeGenPass::process(MatchCaseExprAST* node) {
  CaseState case_state = pop_case();
  case_gen_pass_.set_pattern(case_state.pattern);
  node->condition_->run_pass(&case_gen_pass_);
  Value* condition = case_gen_pass_.result();
  if (!condition) {
    returns (nullptr);
    return;
  }

  // TODO: assuming match cases are exhaustive to avoid overly complicated phi
  //        merging (would have to implement default values for every type)
  if (condition == s_NoBranch || case_state.is_last_case) {
    // jump straight to case true block
    state_.builder.CreateBr(case_state.case_true_block);
  }
  else {
    state_.builder.CreateCondBr(condition,case_state.case_true_block,
                                case_state.case_false_block);
  }

  state_.builder.SetInsertPoint(case_state.case_true_block);

  // generate case expression body
  node->body_->run_pass(this);
  Value* body_value = result();
  if (!body_value) {
    returns (nullptr);
    return;
  }
  // trust the type checker and cast to expected type
  body_value =
    state_.builder.CreateBitOrPointerCast(body_value,
                                          case_state.body_type,
                                          body_value->getName() + ".bitcast");

  // state_.builder.SetInsertPoint(case_true_block);
  // if case matched, jump to after match expression
  state_.builder.CreateBr(case_state.after_block);

  returns (body_value);
}

// MatchExprAST
void CodeGenPass::process(MatchExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);

  node->pattern_->run_pass(this);
  Value* pattern = result();
  if (!pattern || node->match_cases_.empty()) {
    returns (nullptr);
    return;
  }

  Function* function = state_.builder.GetInsertBlock()->getParent();

  BasicBlock* after_block = BasicBlock::Create(state_.llvm_context,
                                               "afterMatchExpr");
  BasicBlock* merge_block = BasicBlock::Create(state_.llvm_context,
                                               "selectCaseExpr");
  size_t idx = 1;
  std::vector<BasicBlock*> case_blocks;
  std::vector<BasicBlock*> case_true_blocks;
  for (size_t i = 0; i < node->match_cases_.size(); ++i) {
    // Create blocks for the match cases
    BasicBlock* case_block = BasicBlock::Create(state_.llvm_context,
                                                "case", function);
    case_blocks.push_back(case_block);
    BasicBlock* case_true_block = BasicBlock::Create(state_.llvm_context,
                                                     "caseMatched",
                                                     function);
    case_true_blocks.push_back(case_true_block);
  }

  state_.builder.CreateBr(case_blocks[0]);

  std::vector<Value*> body_values;
  for (size_t i = 0; i < node->match_cases_.size(); ++i) {
    bool is_last_case = i == node->match_cases_.size()-1;
    auto& match_case = node->match_cases_[i];
    auto case_block = case_blocks[i];
    auto case_true_block = case_true_blocks[i];
    auto next_block = is_last_case ? merge_block : case_blocks[i+1];
    state_.builder.SetInsertPoint(case_block);
    push_case(CaseState(pattern, case_block, case_true_block, next_block,
                        merge_block, get_value_type_dispatch(node),
                        is_last_case));
    match_case->run_pass(this);
    auto body_value = result();
    // match_case->codegen may change current block
    case_true_blocks[i] = state_.builder.GetInsertBlock();
    body_values.push_back(body_value);
  }

  function->getBasicBlockList().push_back(merge_block);
  state_.builder.SetInsertPoint(merge_block);

  PHINode* phi_node = state_.builder.CreatePHI(get_value_type_dispatch(node),
                                               node->match_cases_.size(),
                                               "matchMergeTmp");
  for(size_t i = 0; i < body_values.size(); ++i) {
    // auto body_val_bitcast =
    //     state_.builder.CreateBitOrPointerCast(body_values[i],
    //                                           get_value_type(),
    //                                           body_values[i]->getName()
    //                                           + ".bitcast");
    phi_node->addIncoming(body_values[i], case_true_blocks[i]);
  }

  state_.builder.CreateBr(after_block);

  function->getBasicBlockList().push_back(after_block);
  state_.builder.SetInsertPoint(after_block);

  // current_module_->dump();
  returns (phi_node);
}

// CallExprAST
void CodeGenPass::process(CallExprAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  push_environment(node->Env);
  AutoScope pop_env([node]{ node->Env = pop_environment(); });

  TypeVariable* func_type_var = nullptr;
  auto func = state_.all_functions[node->Callee].get();
  if (func == nullptr) {
    // must be an extern c function
    std::vector<TypeVariable*> arg_types;
    for (auto &arg : node->Args) {
      auto arg_type = resolve_variable(arg->type_var_);
      if (arg_type->type_operator_) {
        auto variant_type =
          get_type_from_constructor(arg_type->type_operator_->type_constructor_);
        if (variant_type) {
          arg_type = variant_type;
        }
      }
      arg_types.push_back(arg_type);
    }
    func_type_var = build_function_type(arg_types);
    // TODO: this may be a bad idea
    auto ret_var = get_function_return_type(func_type_var);
    unify(node->type_var_, ret_var);
  }
  else {
    func_type_var = func->type_var();
  }

  // Look up the name in the global module table.
  std::stringstream type_name;
  type_name << node->Callee << " = " << func_type_var->get_name(false);
  std::string mangled_name = type_name.str();
  Function* CalleeF = state_.get_typeclass_impl_function(node->Callee,
                                                         mangled_name);
  if (!CalleeF) {
    CalleeF = get_function(mangled_name);
    if (!CalleeF) {
      CalleeF = get_function(node->Callee);
      if (!CalleeF) {
        std::cout << "mangled name: " << mangled_name << std::endl;
        logger.error("syntax error", "undefined function referenced");
        returns (nullptr);
        return;
      }
    }
  }

  // check if number of args matches function prototype
  if (CalleeF->arg_size() != node->Args.size()) {
    std::ostringstream msg;
    logger.error("error", msg << "function " << node->Callee << " takes "
                              << CalleeF->arg_size() << " argument(s), but "
                              << node->Args.size() << " were given");
    returns (nullptr);
    return;
  }

  std::vector<Type*> arg_types;
  for (auto &a : CalleeF->args()) {
    arg_types.push_back(a.getType());
  }

  std::vector<Value*> arg_values;
  for (unsigned i = 0, e = node->Args.size(); i != e; ++i) {
    auto &arg_node = node->Args[i];
    arg_node->run_pass(this);
    auto arg = result();
    // trust type checker and cast to generic pointer type for e.g. variant args
    auto arg_bitcast =
      state_.builder.CreateBitOrPointerCast(arg, arg_types[i],
                                            arg->getName() + ".bitcast");

    arg_values.push_back(arg_bitcast);
    if (!arg_values.back()) {
      returns (nullptr);
      return;
    }
  }

  returns (state_.builder.CreateCall(CalleeF, arg_values, "calltmp"));
}

// PrototypeAST
void CodeGenPass::process(PrototypeAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);
  FunctionType* function_type = nullptr;

  std::vector<TypeVariable*> arg_vars = get_function_arg_types(node->type_var_);
  std::vector<Type*> arg_types;
  for (auto &arg : arg_vars) {
    auto type = resolve_variable(arg);
    if (type == FloatType) {
      arg_types.push_back(Type::getDoubleTy(state_.llvm_context));
    }
    else if (type == IntType) {
      arg_types.push_back(Type::getInt64Ty(state_.llvm_context));
    }
    else if (type == BoolType) {
      arg_types.push_back(Type::getInt1Ty(state_.llvm_context));
    }
    else if (type == StringType) {
      arg_types.push_back(Type::getInt8PtrTy(state_.llvm_context));
    }
    else if (auto arg_type = get_value_type(type, is_boxed_type(type))) {
      if (!arg_type->isPointerTy()) {
        arg_type = PointerType::get(arg_type, 0);
      }
      arg_types.push_back(arg_type);
    }
    else {
      std::ostringstream msg;
      logger.error("codegen error", msg << "unknown type " << type->get_name()
                                        << " in prototype for function "
                                        << node->Name);
    }
  }
  Type* return_type = nullptr;
  auto ret_type = resolve_variable(get_function_return_type(node->type_var_));
  if (ret_type == FloatType) {
    return_type = Type::getDoubleTy(state_.llvm_context);
  }
  else if (ret_type == IntType) {
    return_type = Type::getInt64Ty(state_.llvm_context);
  }
  else if (ret_type == BoolType) {
    return_type = Type::getInt1Ty(state_.llvm_context);
  }
  else if (ret_type == StringType) {
    return_type = Type::getInt8PtrTy(state_.llvm_context);
  }
  else if (ret_type == UnitType) {
    return_type = get_value_type(ret_type, is_boxed_type(ret_type));
  }
  else if (auto arg_type = get_value_type(ret_type, is_boxed_type(ret_type))) {
    if (!arg_type->isPointerTy()) {
      arg_type = PointerType::get(arg_type, 0);
    }
    return_type = arg_type;
  }
  else {
    // TODO: this can happen with top-level expressions
    return_type = Type::getVoidTy(state_.llvm_context);
  }
  function_type = FunctionType::get(return_type, arg_types, false);

  Function* function =
      Function::Create(function_type, Function::ExternalLinkage, node->Name,
                       state_.current_module.get());

  // current_module_->dump();
  // set names for all arguments
  unsigned Idx = 0;
  for (auto &Arg : function->args()) {
    Arg.setName(node->Args[Idx++]);
  }

  returns (function);
}

// FunctionAST
void CodeGenPass::process(FunctionAST* node) {
  logger.set_line_column(node->line_num_, node->column_num_);

  if (state_.function_envs.find(node->Proto->getName()) !=
                                state_.function_envs.end()) {
    Function* func_result = nullptr;
    for (auto &func_env : state_.function_envs[node->Proto->getName()]) {
      auto &env = func_env.second;
      push_environment(env);
      if (!is_concrete_type(node->Proto->type_var_)) {
        pop_environment();
        continue;
      }

      std::stringstream type_name;
      type_name << node->Proto->getName()
                << " = "
                << node->type_var()->get_name();
      std::string mangled_name = type_name.str();
      if (state_.function_protos.find(mangled_name) !=
          state_.function_protos.end()) {
        pop_environment();
        continue;
      }

      auto protoAST = *node->Proto;
      state_.function_protos[mangled_name] = copy_proto(protoAST, mangled_name);
      Function* function = get_function(mangled_name);
      if (!function) {
        std::ostringstream msg;
        logger.error("codegen error", msg << "could't find function matching "
                                          << mangled_name);
        returns (nullptr);
        return;
      }

      // Create a new basic block to start insertion into.
      BasicBlock *BB = BasicBlock::Create(state_.llvm_context,
                                          "entry", function);
      state_.builder.SetInsertPoint(BB);

      // Record the function arguments in the named_values_ map.
      state_.named_values.clear();
      for (auto &arg : function->args()) {
        // Create an alloca for this variable.
        auto tvar = node->param_name_to_tvar_[arg.getName()];
        AllocaInst* alloca = create_entry_block_alloca(function, arg.getName(),
                                                       tvar, nullptr, true);

        // Store the initial value into the alloca.
        state_.builder.CreateStore(&arg, alloca);

        // Add arguments to variable symbol table.
        state_.named_values[arg.getName()] = alloca;
        // named_values_[arg.getName()] = &arg;
      }

      node->Body->run_pass(this);
      if (Value* return_val = result()) {
        // Finish off the function.
        state_.builder.CreateRet(return_val);

        // // Validate the generated code, checking for consistency.
        // if (verifyFunction(*function, &errs())) {
        //   state_.current_module_->dump();
        //   logger.error("error", "code generation for function failed");
        //   returns (nullptr);
        //   return;
        // }

        // Run the optimizer on the function.
        state_.function_pass_manager->run(*function);

        // if (DUMP_ASM) {
        // std::string func_name = node->Proto->getName();
        // if (func_name == "describe_language" || func_name == "main") {
        //   state_.current_module->dump();
        // }
        // }

        func_result = function;
      }
      if (func_result == nullptr) {
        logger.error("internal error",
                     "encountered unhandled error during code generation");
        returns (nullptr);
        return;
      }
      pop_environment();
    }
    returns (func_result);
    return;
  }
  // else must be an anonymous expression, or unused function
  if (node->Proto->getName() != "top-level") {
    // not an error, the function is simply not called anywhere
    returns (nullptr);
    return;
  }

  // transfer ownership of the prototype to the state_.function_protos map,
  // but keep a reference to it for use below
  auto &protoAST = *node->Proto;
  state_.function_protos[node->Proto->getName()] = std::move(node->Proto);
  Function *function = get_function(protoAST.getName());
  if (!function) {
    returns (nullptr);
    return;
  }

  // create entry block for the function
  BasicBlock *BB = BasicBlock::Create(state_.llvm_context, "entry", function);
  state_.builder.SetInsertPoint(BB);

  // store the function arguments in this map
  // TODO: this should be a stack of scopes
  state_.named_values.clear();

  node->Body->run_pass(this);
  if (Value* return_val = result()) {
    // finish off the function
    state_.builder.CreateRet(return_val);

    // Validate the generated code, checking for consistency.
    // verifyFunction(*function, &errs());

    // Run the optimizer on the function.
    state_.function_pass_manager->run(*function);

    // if (DUMP_ASM) {
    //   current_module_->dump();
    // }

    returns (function);
    return;
  }

  // error reading body, remove function from module
  function->eraseFromParent();
  returns (nullptr);
}

// TypeAST
void CodeGenPass::process(TypeAST* node) {
  // nothing to do
}

// TypeclassAST
void CodeGenPass::process(TypeclassAST* node) {
  for (auto &impl : node->impls) {
    impl->run_pass(this);
  }
}

// TypeclassImplAST
void CodeGenPass::process(TypeclassImplAST* node) {
  for (auto &method_entry : node->methods_) {
    auto &method = method_entry.second;
    std::string method_name = method_entry.first;
    method->run_pass(this);
    if (auto func = result()) {
      node->method_funcs_[method_name] = (Function*)func;
    }
    else {
      return;
    }
  }
}

CodeGenPass::CodeGenPass(ModuleState &state)
  : state_(state), case_gen_pass_(this, state), last_value_(nullptr),
    case_state_push_count(0)
{
}

// we need a way to retrieve the output of the last instruction
// so we cache the result to use as a return value
void CodeGenPass::returns(Value* value) {
  assert(last_value_ == nullptr);
  last_value_ = value;
}

// returns the cached return value
Value* CodeGenPass::result() {
  auto result = last_value_;
  last_value_ = nullptr;
  return result;
}

Type* CodeGenPass::get_value_type(TypeVariable* type_var, bool ptr_type) {
    type_var = resolve_variable(type_var);
    auto tcon_operator = type_var->type_operator_;
    if (type_var == FloatType) {
      return Type::getDoubleTy(state_.llvm_context);
    }
    else if (type_var == IntType) {
      return Type::getInt64Ty(state_.llvm_context);
    }
    else if (type_var == BoolType) {
      return Type::getInt1Ty(state_.llvm_context);
    }
    else if (type_var == StringType) {
      return Type::getInt8PtrTy(state_.llvm_context);
    }
    else if (auto val_type = get_value_type(tcon_operator, type_var)) {
      auto tname = type_var->get_name();
      // Type* val_type = state_.struct_map[tname];
      if (!val_type) {
        val_type = (Type*)StructType::create(state_.llvm_context,
                                             "struct." +
                                             tcon_operator->type_constructor_);
      }
      // val_type->dump();
      if (ptr_type && !val_type->isPointerTy()) {
        val_type = PointerType::get(val_type, 0);
      }
      return val_type;
    }
    else {
      return nullptr;
    }
}

Type* CodeGenPass::get_value_type(TypeOperator* type_op,
                                  TypeVariable* type_var) {
  if (!type_op) {
    return nullptr;
  }

  auto tname = type_var->get_name();
  StructType* structReg = state_.struct_map[tname];
  if (!structReg) {
    std::vector<Type*> members;
    // constructor enum
    members.push_back(Type::getInt32Ty(state_.llvm_context));
    // don't create members for generic variant
    if (type_op->type_constructor_ != " | ") {
      for (auto type_var : type_op->types_) {
        // if product type, add individual members
        if (type_var->type_operator_
            && type_var->type_operator_->type_constructor_ == " * ") {
          for (auto member : type_var->type_operator_->types_) {
            if (auto val_type = get_value_type(member, is_boxed_type(member))) {
              members.push_back(val_type);
            }
            else {
              std::ostringstream msg;
              logger.error("codegen error", msg << "failed to allocate type " <<
                           member->get_name() << " for constructor " <<
                           type_op->type_constructor_);
              return nullptr;
            }
          }
          continue;
        }
        if (auto val_type = get_value_type(type_var, is_boxed_type(type_var))) {
          members.push_back(val_type);
        }
        else {
          std::ostringstream msg;
          logger.error("codegen error", msg << "failed to allocate type " <<
                       type_var->get_name() << " for constructor " <<
                       type_op->type_constructor_);
          return nullptr;
        }
      }
    }
    auto variant_struct = StructType::create(state_.llvm_context, members,
                                             "struct." + tname);
    state_.struct_map[tname] = variant_struct;
    return variant_struct;
  }
  return structReg;
}

// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
// the function.  This is used for mutable variables etc.
AllocaInst* CodeGenPass::create_entry_block_alloca(Function* function,
                                                   const std::string &VarName,
                                                   TypeVariable* var_type,
                                                   ExprAST* var_expr,
                                                   bool use_ptr) {
  IRBuilder<> TmpB(&function->getEntryBlock(),
                 function->getEntryBlock().begin());
  auto type_var = resolve_variable(var_type);
  auto tcon_operator = type_var->type_operator_;
  if (type_var == FloatType) {
    return TmpB.CreateAlloca(Type::getDoubleTy(state_.llvm_context), 0,
                            VarName.c_str());
  }
  else if (type_var == IntType) {
    return TmpB.CreateAlloca(Type::getInt64Ty(state_.llvm_context), 0,
                            VarName.c_str());
  }
  else if (type_var == BoolType) {
    return TmpB.CreateAlloca(Type::getInt1Ty(state_.llvm_context), 0,
                            VarName.c_str());
  }
  else if (type_var == StringType) {
    return TmpB.CreateAlloca(Type::getInt8PtrTy(state_.llvm_context), 0,
                            VarName.c_str());
  }
  else if (var_expr) {
    auto val_type = get_value_type_dispatch(var_expr);
    if (use_ptr && !val_type->isPointerTy()) {
      val_type = PointerType::get(val_type, 0);
      return TmpB.CreateAlloca(val_type, 0, VarName.c_str());
    }
    else {
      return TmpB.CreateAlloca(val_type, 0, VarName.c_str());
    }
  }
  else if (auto val_type = get_value_type(tcon_operator, type_var)) {
    if (use_ptr && !val_type->isPointerTy()) {
      val_type = PointerType::get(val_type, 0);
      return TmpB.CreateAlloca(val_type, 0, VarName.c_str());
    }
    else {
      // shouldn't ever get here
      assert(false);
      return TmpB.CreateAlloca(val_type, 0, VarName.c_str());
    }
  }
  else {
    std::ostringstream msg;
    logger.error("codegen error", msg <<
                 "requested stack allocation for variable " << VarName <<
                 " with unknown type " << type_var->get_name());
    return nullptr;
  }
}

void CodeGenPass::push_case(CaseState state) {
  assert(case_state_push_count++ == 0);
  priv_case_state_ = state;
}

CodeGenPass::CaseState CodeGenPass::pop_case() {
  assert(--case_state_push_count == 0);
  return priv_case_state_;
}

Type* CodeGenPass::get_value_type(ExprAST* node) {
  return get_value_type(node->type_var_, node->is_boxed());
}

Type* CodeGenPass::get_value_type(NumberExprAST* node) {
  return Type::getDoubleTy(state_.llvm_context);
}

Type* CodeGenPass::get_value_type(IntegerExprAST* node) {
  return Type::getInt64Ty(state_.llvm_context);
}

Type* CodeGenPass::get_value_type(BoolExprAST* node) {
  return Type::getInt1Ty(state_.llvm_context);
}

Type* CodeGenPass::get_value_type(UnitExprAST* node) {
  auto tname = node->type_var_->get_name();
  StructType* structReg = state_.struct_map[tname];
  if (!structReg) {
    std::cout << "Unit type '()' unexpectedly not defined yet" << std::endl;
  }
  return structReg;
}

Type* CodeGenPass::get_value_type_dispatch(ExprAST* node) {
  // TODO: this ugliness is from a refactor, need to rethink
  NumberExprAST* num_expr = dynamic_cast<NumberExprAST*>(node);
  IntegerExprAST* int_expr = dynamic_cast<IntegerExprAST*>(node);
  BoolExprAST* bool_expr = dynamic_cast<BoolExprAST*>(node);
  UnitExprAST* unit_expr = dynamic_cast<UnitExprAST*>(node);
  ValueConstructorExprAST* val_constructor_expr =
    dynamic_cast<ValueConstructorExprAST*>(node);

  Type* node_type = nullptr;
  if (num_expr) {
    node_type = get_value_type(num_expr);
  }
  else if (int_expr) {
    node_type = get_value_type(int_expr);
  }
  else if (bool_expr) {
    node_type = get_value_type(bool_expr);
  }
  else if (unit_expr) {
    node_type = get_value_type(unit_expr);
  }
  else if (val_constructor_expr) {
    node_type = get_value_type(val_constructor_expr);
  }
  else {
    node_type = get_value_type(node);
  }
  return node_type;
}

Type* CodeGenPass::get_value_type(ValueConstructorExprAST* node) {
  node->push_type_environment();

  auto tname = node->type_var_->get_name();
  StructType* structReg = state_.struct_map[tname];
  if (true) {//!structReg) {
    std::vector<Type*> members;
    members.push_back(Type::getInt32Ty(state_.llvm_context));
    for (auto &arg : node->tcon_args_) {
      try {
        members.push_back(get_value_type_dispatch(arg.get()));
      } catch(...) {
        std::cout << node->constructor_ << " failed with arg "
                  << arg->type_var_->get_name() << std::endl;
      }
    }
    structReg = StructType::create(state_.llvm_context, members,
                                   "struct." + tname);
    state_.struct_map[tname] = structReg;
  }
  node->pop_type_environment();

  // structReg->dump();
  return PointerType::get(structReg, 0);
}


/*----------------------------------------------------------------------------*\
|* CaseGenPass
L*----------------------------------------------------------------------------*/

// NumberExprAST
void CaseGenPass::process(NumberExprAST* node) {
  node->run_pass(codegen_);
  Value* num_value = codegen_->result();
  returns (state_.builder.CreateFCmpUEQ(num_value, pattern_, "cmptmp"));
}

// IntegerExprAST
void CaseGenPass::process(IntegerExprAST* node) {
  node->run_pass(codegen_);
  Value* num_value = codegen_->result();
  returns (state_.builder.CreateICmpEQ(num_value, pattern_, "cmptmp"));
}

// StringExprAST
void CaseGenPass::process(StringExprAST* node) {
  node->run_pass(codegen_);
  Value* str_value = codegen_->result();

  std::vector<Value*> call_args;
  call_args.push_back(str_value);
  call_args.push_back(pattern_);

  // lookup string equality function
  auto string_cmp = codegen_->get_function("str_eq");
  if (!string_cmp) {
    std::ostringstream msg;
    logger.error("internal error", msg << "missing str_eq implementation");
    returns (nullptr);
    return;
  }

  returns (state_.builder.CreateCall(string_cmp, call_args, "str_eq"));
}

// BoolExprAST
void CaseGenPass::process(BoolExprAST* node) {
  node->run_pass(codegen_);
  Value* num_value = codegen_->result();
  returns (state_.builder.CreateICmpEQ(num_value, pattern_, "cmptmp"));
}

// UnitExprAST
void CaseGenPass::process(UnitExprAST* node) {
}

// VariableExprAST
void CaseGenPass::process(VariableExprAST* node) {
  // look up variable by name
  // TODO: need to push/pop scope to prevent variable leaking
  //       out of case expression
  Value* variable = state_.named_values[node->Name];
  if (!variable) {
    auto function = state_.builder.GetInsertBlock()->getParent();
    // Create an alloca for this variable.
    auto alloca_var =
      codegen_->create_entry_block_alloca(function, node->Name, node->type_var_,
                                          node, node->is_boxed());

    // Add to variable symbol table
    state_.named_values[node->Name] = alloca_var;
    variable = alloca_var;
  }

  state_.builder.CreateStore(pattern_, variable);

  // returning s_NoBranch signals that there is no conditional branch needed
  // TODO: this is a bit dangerous
  returns (s_NoBranch);
}

// ValueConstructorExprAST
void CaseGenPass::process(ValueConstructorExprAST* node) {
  node->push_type_environment();
  // AutoScope pop_env([this]{ pop_environment(); });

  auto entry_block = state_.builder.GetInsertBlock();
  Function* function = entry_block->getParent();

  auto tname = node->type_var_->get_name();
  StructType* variant_struct = state_.struct_map[tname];
  if (true) {//!variant_struct) {
    // if type wasn't already registered, build here
    std::vector<Type*> members;
    members.push_back(Type::getInt32Ty(state_.llvm_context));
    for (auto &arg : node->tcon_args_) {
      try {
        members.push_back(codegen_->get_value_type_dispatch(arg.get()));
      } catch(std::exception &ex) {
        std::cout << node->constructor_ << " failed with arg "
                  << arg->type_var_->get_name() << std::endl;
        std::cout << ex.what() << std::endl;
      }
    }
    variant_struct = StructType::create(state_.llvm_context, members,
                                        "struct." + tname);
    state_.struct_map[tname] = variant_struct;
  }
  node->pop_type_environment();

  // look up constructor index for this match case
  auto tcon_enum = get_constructor_value(node->constructor_);
  auto case_type_val =
    ConstantInt::get(state_.llvm_context,
                     APInt(/*nbits*/32, tcon_enum, /*is_signed*/false));

  // get constants for our indices
  auto el_idx0 = ConstantInt::get(state_.llvm_context,
                                  APInt(/*nbits*/32, 0, /*is_signed*/false));

  // first index (0) dereferences pointer to struct,
  // second index (0) points at constructor index
  //  (differentiating e.g. Some constructor vs. None)
  std::vector<Value*> indices;
  indices.push_back(el_idx0);
  indices.push_back(el_idx0);

  // grab pointer to constructor index for variant we're matching against
  Value* patt_type_ptr = state_.builder.CreateGEP(variant_struct, pattern_,
                                                  indices);
  // load constructor index for match input
  auto patt_type_val = state_.builder.CreateLoad(patt_type_ptr);
  // compare constructor index against our match case
  auto condition = state_.builder.CreateICmpEQ(case_type_val, patt_type_val,
                                               "cmpvcon");

  // if constructor is simple enum with no args (e.g. type bool = True | False)
  // return whether the constructor index matched
  if (node->tcon_args_.empty()) {
    returns (condition);
    return;
  }
  // else we have more to compare (e.g. Some(n))

  // Create blocks for the constructor body test
  // will jump here if constructor index matched (e.g. Some == Some)
  BasicBlock* case_block =
    BasicBlock::Create(state_.llvm_context,
                       "variantTypeMatched("
                       + std::to_string(node->line_num_)
                       + ")",
                       function);
  // will jump here from every possible failure point, as well as
  //  from the last passing test.
  // all conditions merge into a final phi node in this block
  // which determines whether this case matched or not
  BasicBlock* merge_tests_block =
    BasicBlock::Create(state_.llvm_context,
                       "variantCaseMerge("
                       + std::to_string(node->line_num_)
                       + ")");

  // emit branch for constructor index match
  state_.builder.CreateCondBr(condition, case_block, merge_tests_block);

  // switch to successful index match block
  state_.builder.SetInsertPoint(case_block);

  std::vector<std::pair<Value*, BasicBlock*>> branches;
  for (size_t i = 0; i < node->tcon_args_.size(); ++i) {
    // first index (0) dereferences pointer to struct,
    // second index (1) points at first constructor arg (e.g. "n" in Some(n))
    indices.clear();
    indices.push_back(el_idx0);
    auto el_idx1 = ConstantInt::get(state_.llvm_context,
                                    APInt(/*nbits*/32, i+1,
                                          /*is_signed*/false));
    indices.push_back(el_idx1);
    // grab pointer to first constructor arg in match input
    Value* patt_arg_ptr = state_.builder.CreateGEP(variant_struct,
                                                   pattern_, indices);
    // load first constructor arg
    auto patt_arg_val = state_.builder.CreateLoad(patt_arg_ptr);
    // recurse - handle as independent case, and return resulting condition
    node->push_type_environment();
    Value* arg_condition = nullptr;
    // gen code for the sub-pattern
    // arg_condition = node->tcon_args_[i]->case_codegen(patt_arg_val);
    // store parent pattern
    auto prev_pattern = pattern_;
    pattern_ = patt_arg_val;
    node->tcon_args_[i]->run_pass(this);
    // restore pattern
    pattern_ = prev_pattern;
    // grab result of sub-pattern test
    arg_condition = result();

    if (arg_condition == s_NoBranch) {
      arg_condition = ConstantInt::getTrue(state_.llvm_context);
    }

    // reset back to case block in case the sub-pattern
    //  changed the current block
    case_block = state_.builder.GetInsertBlock();
    // if (arg_condition != s_NoBranch) {
      branches.push_back(std::make_pair(arg_condition, case_block));
    // }
    node->pop_type_environment();
    if (i < node->tcon_args_.size()-1) {
      case_block = BasicBlock::Create(state_.llvm_context,
                                      "variantArgTest", function);
      // emit branch for constructor arg match
      state_.builder.CreateCondBr(arg_condition, case_block, merge_tests_block);
      // switch to successful arg match block
      state_.builder.SetInsertPoint(case_block);
    }
    else {
        state_.builder.CreateBr(merge_tests_block);
    }
  }

  // set up for the next case
  function->getBasicBlockList().push_back(merge_tests_block);
  state_.builder.SetInsertPoint(merge_tests_block);

  // merge results of all tests in phi node
  PHINode* phi_node =
    state_.builder.CreatePHI(Type::getInt1Ty(state_.llvm_context),
                             1 + node->tcon_args_.size(),
                             "vconArgPhiTmp");
  phi_node->addIncoming(condition, entry_block);
  for (auto& branch : branches) {
    if (branch.first == s_NoBranch) {
      continue;
    }
    phi_node->addIncoming(branch.first, branch.second);
  }

  // our phi node captures the result of all of the parts of our case pattern
  returns (phi_node);
}

// UnaryExprAST
void CaseGenPass::process(UnaryExprAST* node) {
}

// BinaryExprAST
void CaseGenPass::process(BinaryExprAST* node) {
}

// IfExprAST
void CaseGenPass::process(IfExprAST* node) {
}

// MatchCaseExprAST
void CaseGenPass::process(MatchCaseExprAST* node) {
}

// MatchExprAST
void CaseGenPass::process(MatchExprAST* node) {
}

// CallExprAST
void CaseGenPass::process(CallExprAST* node) {
}

// PrototypeAST
void CaseGenPass::process(PrototypeAST* node) {
}

// FunctionAST
void CaseGenPass::process(FunctionAST* node) {
}

// TypeAST
void CaseGenPass::process(TypeAST* node) {
}

// TypeclassAST
void CaseGenPass::process(TypeclassAST* node) {
}

// TypeclassImplAST
void CaseGenPass::process(TypeclassImplAST* node) {
}



} // namespace bon
