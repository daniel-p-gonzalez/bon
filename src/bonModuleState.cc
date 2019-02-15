/*----------------------------------------------------------------------------*\
|*
|* State associated with a module compile pass
|*
L*----------------------------------------------------------------------------*/
#include "bonModuleState.h"
#include "auto_scope.h"

namespace bon {

FunctionAST*
  ModuleState::get_typeclass_impl_function_node(std::string method_name,
                                                TypeVariable* func_type_var) {
  if (method_to_typeclass.find(method_name) == method_to_typeclass.end()) {
    return nullptr;
  }
  std::string typeclass_name = method_to_typeclass[method_name];
  auto &typeclass = typeclasses[typeclass_name];
  for (auto &impl : typeclass->impls) {
    auto &method = impl->methods_[method_name];
    if (!method) {
      continue;
    }
    push_environment(method->type_env_);
    AutoScope pop_env([this, &method]{
      pop_environment();
    });
    auto method_type_var = resolve_variable(method->type_var(), false);
    if (method_type_var->type_operator_ && func_type_var->type_operator_) {
      if (can_unify(method_type_var->type_operator_,
                               func_type_var->type_operator_)) {
        return method.get();
      }
    }
  }
  return nullptr;
}

Function* ModuleState::get_typeclass_impl_function(std::string func_name,
                                                   std::string mangled_name) {
  auto typeclass_entry = method_to_typeclass.find(func_name);
  if (typeclass_entry != method_to_typeclass.end()) {
    std::string typeclass_name = typeclass_entry->second;
    for (auto &impl : typeclasses[typeclass_name]->impls) {
      auto &method = impl->methods_[func_name];
      if (!method) {
        continue;
      }
      std::string impl_name = func_name + " = "
                              + method->type_var()->get_name();
      if (impl_name == mangled_name) {
        if (auto* func = current_module->getFunction(impl_name)) {
          return func;
        }
        return nullptr; // impl->method_funcs_[func_name];
      }
    }
  }

  return nullptr;
}

Function* ModuleState::get_function(std::string func_name) {
  if (auto* func = current_module->getFunction(func_name)) {
    return func;
  }

  // // check existing function prototype
  // auto func_proto = function_protos.find(func_name);
  // if (func_proto != function_protos.end()) {
  //   return func_proto->second->codegen();
  // }

  // not found
  return nullptr;
}


ModuleState::ModuleState()
 : builder(llvm_context)
{
}

} // namespace bon
