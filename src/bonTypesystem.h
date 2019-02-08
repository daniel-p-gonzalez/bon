/*----------------------------------------------------------------------------*\
|*
|* Type System:
|*        Typing primitives used by bon::TypeAnalysis pass for
|*       type inference and type checking.
|*
L*----------------------------------------------------------------------------*/

#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>

namespace bon {

class TypeVariable;
// type environment is a map from type name to type variable
typedef std::map<std::string, TypeVariable*> TypeEnv;
// map from program variable name to type variable
typedef std::map<std::string, TypeVariable*> TypeMap;
typedef std::set<TypeVariable*> TypeVariableSet;
typedef std::map<std::string, uint32_t> IndexMap;

class TypeNameGenerator {
public:
    std::string type_names;
    int type_count = 0;

    TypeNameGenerator() {
        type_names = "abcdefghijklmnopqrstuvwxyz";
    }

    void reset() {
        type_count = 0;
    }

    std::string new_type_name() {
        char type_char = type_names[type_count%26];
        type_count += 1;
        std::string type_name = "'";
        type_name += type_char;
        if (type_count / 26 > 0) {
            type_name += std::to_string(type_count / 26);
        }
        return type_name;
    }
};

class TypeOperator {
public:
    std::string type_constructor_;
    std::vector<TypeVariable*> types_;
    TypeOperator(std::string type_constructor,
                 std::vector<TypeVariable*> &types)
    : type_constructor_(type_constructor), types_(types)
    {
    }

    std::string to_string(std::set<TypeVariable*> &occurs, bool store_name);
};

class TypeVariable {
public:
    TypeOperator* type_operator_;
    TypeVariable* parent_;
    std::string type_name_;
    std::string variant_name_;
    TypeVariable(TypeOperator* type_operator=nullptr)
    : type_operator_(type_operator), parent_(nullptr), type_name_("")
    {
    }

    std::string get_name(bool store_name=true,
                         TypeVariableSet occurs=TypeVariableSet());
    TypeVariable* get_root();

    // set new type variable root
    void set_type(TypeVariable* new_type);
};

void push_environment(std::map<std::string, TypeVariable*> &env);
std::map<std::string, TypeVariable*> pop_environment();
void dump_environment();
void reset_type_variables();

void push_typeclass_environment(TypeEnv &env);
TypeEnv pop_typeclass_environment();

void register_type(std::string type_name, TypeVariable* tvar);

// Resolve variable to its root, inside current typing environment.
// If update_environment==true, add fresh variable to
//  environment for later substitution.
// set update_environment to false if type checking (not inferring)
TypeVariable* resolve_variable(TypeVariable* type_var,
                               bool update_environment=true);
TypeVariable* flatten_variable(TypeVariable* type_var);
void get_fresh_variable(TypeVariable* type_var);
TypeVariable* gen_variable(TypeVariable* type_var);

bool type_operators_match(TypeOperator* lhs, TypeOperator* rhs);
//void unify_type_operators(TypeOperator* lhs, TypeOperator* rhs);
void unify(TypeVariable* lhs, TypeVariable* rhs);
TypeVariable* build_function_type(std::vector<TypeVariable*> &param_types,
                                  TypeVariable* ret_type=nullptr);
TypeVariable* build_tuple_type(std::vector<TypeVariable*> &param_types);

TypeVariable* build_variant_type(TypeVariable* v_type,
                                 TypeMap &variant_types,
                                 IndexMap &field_map);
TypeVariable* get_type_from_constructor(std::string constructor);
std::string get_constructor_from_type(TypeVariable* type);
TypeVariable* build_from_type_constructor(std::string constructor,
                                          std::vector<TypeVariable*> &types);
uint32_t get_constructor_value(std::string constructor);
uint32_t get_constructor_field_index(std::string constructor,
                                     std::string field);

bool is_boxed_type(TypeVariable* type_var);
bool is_concrete_type(TypeVariable* type_var);
bool is_pointer_type(TypeVariable* type_var);
TypeVariable* get_type_of_pointer(TypeVariable* type_var);

TypeVariable* get_function_return_type(TypeVariable* func_type);
std::vector<TypeVariable*> get_function_arg_types(TypeVariable* func_type);
TypeVariable* type_variable_from_identifier(std::string ident);

extern TypeVariable* IntType;
extern TypeVariable* FloatType;
extern TypeVariable* StringType;
extern TypeVariable* BoolType;
extern TypeVariable* UnitType;
extern TypeVariable* PointerType;

} // namespace bon
