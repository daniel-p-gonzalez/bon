/*----------------------------------------------------------------------------*\
|*
|* State associated with a module compile pass
|*
L*----------------------------------------------------------------------------*/

#pragma once
#include "bonTypesystem.h"
#include "bonAST.h"
#include "llvm/IR/IRBuilder.h"
#include "bonJIT.h"

#include <map>
#include <string>
#include <memory>

namespace bon {

struct ModuleState {
  typedef std::pair<std::string, TypeEnv> FuncTypeEnv;
  std::map<std::string, std::vector<FuncTypeEnv>> function_envs;
  std::string filename;
  std::map<std::string, std::string> method_to_typeclass;
  std::map<std::string, TypeclassASTPtr> typeclasses;
  std::map<std::string, FunctionASTPtr> all_functions;
  std::map<std::string, PrototypeASTPtr> function_protos;
  std::vector<std::string> function_names;
  std::vector<std::unique_ptr<FunctionAST>> toplevel_expressions;
  // array of all functions (including impl functions)
  //  in the order they were defined (simplifies code gen)
  std::vector<FunctionAST*> ordered_functions;

  // TODO: pull out LLVM state into another object
  LLVMContext llvm_context;
  IRBuilder<> builder;
  std::unique_ptr<Module> current_module;
  std::map<std::string, AllocaInst *> named_values;
  std::unique_ptr<legacy::FunctionPassManager> function_pass_manager;
  std::unique_ptr<BonJIT> JIT;
  std::map<std::string, StructType*> struct_map;

  ModuleState();
  FunctionAST* get_typeclass_impl_function_node(std::string method_name,
                                                TypeVariable* func_type_var);
  Function* get_typeclass_impl_function(std::string func_name,
                                        std::string mangled_name);
  Function* get_function(std::string func_name);
};

} // namespace bon
