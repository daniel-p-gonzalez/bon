/*----------------------------------------------------------------------------*\
|*
|* Code Gen AST walker for generating machine code using LLVM
|*
L*----------------------------------------------------------------------------*/

#pragma once
#include "bonCompilerPass.h"
#include "bonModuleState.h"

#include <string>
#include <map>

namespace bon {

class CodeGenPass;

// code generator for pattern match cases
class CaseGenPass : public CompilerPass {
public:
  void process(NumberExprAST* node) override;
  void process(IntegerExprAST* node) override;
  void process(StringExprAST* node) override;
  void process(BoolExprAST* node) override;
  void process(UnitExprAST* node) override;
  void process(VariableExprAST* node) override;
  void process(ValueConstructorExprAST* node) override;
  void process(UnaryExprAST* node) override;
  void process(BinaryExprAST* node) override;
  void process(IfExprAST* node) override;
  void process(WhileExprAST* node) override;
  void process(MatchCaseExprAST* node) override;
  void process(MatchExprAST* node) override;
  void process(CallExprAST* node) override;
  void process(SizeofExprAST* node) override;
  void process(PtrOffsetExprAST* node) override;
  void process(PrototypeAST* node) override;
  void process(FunctionAST* node) override;
  void process(TypeAST* node) override;
  void process(TypeclassAST* node) override;
  void process(TypeclassImplAST* node) override;

  CaseGenPass(CodeGenPass* codegen, ModuleState &state)
    : codegen_(codegen), state_(state) {}

  void set_pattern(Value* pattern) {pattern_ = pattern;}

private:
  friend CodeGenPass;
  CodeGenPass* codegen_;
  ModuleState &state_;
  Value* pattern_;

  // we need a way to retrieve the output of the last instruction
  // so we cache the result to use as a return value
  void returns(Value* value) { last_value_ = value; }
  // returns the cached return value
  Value* result() { return last_value_; }
  // return value for the last "process" call
  Value* last_value_;
};

class CodeGenPass : public CompilerPass {
public:
  void process(NumberExprAST* node) override;
  void process(IntegerExprAST* node) override;
  void process(StringExprAST* node) override;
  void process(BoolExprAST* node) override;
  void process(UnitExprAST* node) override;
  void process(VariableExprAST* node) override;
  void process(ValueConstructorExprAST* node) override;
  void process(UnaryExprAST* node) override;
  void process(BinaryExprAST* node) override;
  void process(IfExprAST* node) override;
  void process(WhileExprAST* node) override;
  void process(MatchCaseExprAST* node) override;
  void process(MatchExprAST* node) override;
  void process(CallExprAST* node) override;
  void process(SizeofExprAST* node) override;
  void process(PtrOffsetExprAST* node) override;
  void process(PrototypeAST* node) override;
  void process(FunctionAST* node) override;
  void process(TypeAST* node) override;
  void process(TypeclassAST* node) override;
  void process(TypeclassImplAST* node) override;

  CodeGenPass(ModuleState &state);

private:
  friend CaseGenPass;
  ModuleState &state_;
  CaseGenPass case_gen_pass_;
  // variable name to line number it was moved on
  std::map<std::string, DocPosition> moved_vars_;
  // allocations to free at end of scope
  std::set<Value*> free_list_;
  // parameters are borrowed - don't free
  std::set<Value*> borrowed_list_;
  std::set<Value*> child_mem_list_;
  // polymorphic destructors to generate
  std::map<std::string, std::vector<TypeVariable*>> destructor_list_;
  // if we're generating destructors, make sure not to recurse
  bool in_destructor_;
  // inside codegen for a constructor?
  // needed for managing memory ownership
  bool in_constructor_;

  struct CaseState {
    Value* pattern;
    BasicBlock* case_block;
    BasicBlock* case_true_block;
    BasicBlock* case_false_block;
    BasicBlock* after_block;
    Type* body_type;
    bool is_last_case;
    CaseState() {}
    CaseState(Value* _pattern, BasicBlock* _case_block,
              BasicBlock* _case_true_block, BasicBlock* _case_false_block,
              BasicBlock* _after_block, Type* _body_type, bool _is_last_case)
      : pattern(_pattern), case_block(_case_block),
        case_true_block(_case_true_block), case_false_block(_case_false_block),
        after_block(_after_block), body_type(_body_type),
        is_last_case(_is_last_case) {}
  };

  // not to be directly used
  CaseState priv_case_state_;
  // sanity check
  int case_state_push_count;

  void push_case(CaseState state);
  CaseState pop_case();

  Function* get_function(std::string name);

  TypeVariable* fn_type_from_call(CallExprAST* node);

  Type* get_value_type_dispatch(ExprAST* node);
  Type* get_value_type(TypeOperator* type_op, TypeVariable* type_var);
  Type* get_value_type(TypeVariable* type_var, bool ptr_type=false);
  Type* get_value_type(ExprAST* node);
  Type* get_value_type(NumberExprAST* node);
  Type* get_value_type(IntegerExprAST* node);
  Type* get_value_type(BoolExprAST* node);
  Type* get_value_type(UnitExprAST* node);
  Type* get_value_type(ValueConstructorExprAST* node);

  // creates an alloca instruction in the entry block of
  // the function for variables living on the stack
  AllocaInst *create_entry_block_alloca(Function* function,
                                        const std::string &var_name,
                                        TypeVariable* var_type,
                                        ExprAST* var_expr=nullptr,
                                        bool use_ptr=false);

  // free memory associated with constructed object
  void free_obj(Value* obj_ptr, bool is_child_obj);
  std::map<std::string, Value*> tracked_allocs_;
  std::map<Value*, TypeVariable*> alloc_types_;
  // we need a way to retrieve the output of the last instruction
  // so we cache the result to use as a return value
  void returns(ExprAST* node, Value* value);
  // return value for the last "process" call
  Value* last_value_;
public:
  // returns the cached return value
  Value* result();
};

} // namespace bon
