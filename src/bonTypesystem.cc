/*----------------------------------------------------------------------------*\
|*
|* Type System:
|*        Typing primitives used by bon::TypeAnalysis pass for
|*       type inference and type checking.
|*
L*----------------------------------------------------------------------------*/

#include "bonTypesystem.h"
#include "term_colors.h"
#include "bonLogger.h"

#include <iostream>
#include <stdexcept>

namespace bon {

static TypeNameGenerator s_type_name_gen;
static std::vector<TypeVariable*> s_empty_types;
static TypeEnv s_type_env;
static std::vector<TypeEnv> s_env_stack;
// registry of user defined types
static TypeEnv s_type_registry;
static TypeEnv s_type_constructors;
// typing environment for class type variables
static TypeEnv s_typeclass_env;
static IndexMap s_constructor_values;
// map from constructor name to map from field name to field index
static std::map<std::string, IndexMap> s_constructor_field_indices;

TypeVariable* IntType =
                new TypeVariable(new TypeOperator("int", s_empty_types));
TypeVariable* FloatType =
                new TypeVariable(new TypeOperator("float", s_empty_types));
TypeVariable* StringType =
                new TypeVariable(new TypeOperator("string", s_empty_types));
TypeVariable* BoolType =
                new TypeVariable(new TypeOperator("bool", s_empty_types));
TypeVariable* UnitType =
                new TypeVariable(new TypeOperator("()", s_empty_types));

void dump_environment() {
    std::cout << "Environment state:" << std::endl;
    for (size_t i = 0; i < s_env_stack.size(); ++i) {
        std::cout << "Environment #" << i << ":" << std::endl;
        for (auto &entry : s_env_stack[i]) {
            std::cout << entry.first << std::endl;
        }
    }
    std::cout << "Environment #" << s_env_stack.size() << ":" << std::endl;
    for (auto &entry : s_type_env) {
        std::cout << entry.first << std::endl;
    }
}

void push_environment(std::map<std::string, TypeVariable*> &env) {
    // s_type_name_gen.reset();
    s_env_stack.push_back(s_type_env);
    for (auto &entry : env) {
        s_type_env[entry.first] = entry.second;
    }
    // this doesn't overwrite:
    // s_type_env.insert(env.begin(), env.end());

    // s_type_env = env;
}

TypeEnv pop_environment() {
    TypeEnv env_copy = s_type_env;
    if (s_env_stack.size() > 0){
        s_type_env = s_env_stack.back();
        s_env_stack.pop_back();
    }
    else {
        s_type_env.clear();
    }
    return env_copy;
}

void push_typeclass_environment(TypeEnv &env) {
    s_typeclass_env = env;
}

TypeEnv pop_typeclass_environment() {
    return s_typeclass_env;
}

void register_type(std::string type_name, TypeVariable* tvar) {
    if (s_type_registry.find(type_name) != s_type_registry.end()) {
        bon::logger.error("type error", "type name already exists");
        return;
    }
    s_type_registry[type_name] = tvar;
}

std::string TypeOperator::to_string(TypeVariableSet &occurs,
                                    bool store_name) {
    if (types_.empty()) {
        return type_constructor_;
    }
    if (types_.size() == 1) {
        return type_constructor_ + " " + types_[0]->get_name(store_name, occurs);
    }
    std::string type_string = types_[0]->get_name(store_name, occurs);
    for (size_t i = 1; i < types_.size(); ++i) {
        type_string += type_constructor_
                       + types_[i]->get_name(store_name, occurs);
    }
    return type_string;
}

TypeVariable* get_type_from_constructor(std::string constructor);

std::string _get_variant_name(TypeVariable* type_var) {
    type_var = resolve_variable(type_var);
    if (type_var->type_operator_ == nullptr) {
        return "";
    }

    auto op = type_var->type_operator_->type_constructor_;
    if (isupper(type_var->type_operator_->type_constructor_[0])) {
        // this is a constructor - return the name of the variant it belongs to
        return get_type_from_constructor(op)->variant_name_;
    }
    else {
        for (auto& type : type_var->type_operator_->types_) {
            std::string name = _get_variant_name(type);
            if (name != "") {
                return name;
            }
        }
    }

    return "";
}

std::string get_variant_name(TypeVariable* type_var) {
    return _get_variant_name(type_var);
}

std::string TypeVariable::get_name(bool store_name, TypeVariableSet occurs) {
    // check for recursion
    bool in_registry = s_type_registry.find(variant_name_)
                       != s_type_registry.end();
    // if (occurs.count(this) > 0 && in_registry) {
    if (occurs.size() > 0 && in_registry) {
        return variant_name_;
    }
    if (occurs.count(this) > 0) {
        return _get_variant_name(this);
    }
    occurs.insert(this);

    auto type_node = resolve_variable(this); //get_root();
    if (type_node->type_name_ != "") {
        return type_node->type_name_;
    }
    else if (type_node->type_operator_ != nullptr) {
        // don't store type operator names:
        //  having a name communicates that it is a
        //  free variable (for better or worse)
        // type_node->type_name_ = type_node->type_operator_->to_string();
        return type_node->type_operator_->to_string(occurs, store_name);
    }
    else {
        auto type_name = s_type_name_gen.new_type_name();
        if (store_name) {
            type_node->type_name_ = type_name;
        }
        return type_name;
    }
}

TypeVariable* TypeVariable::get_root() {
    auto type_node = this;

    // find root
    while (type_node->parent_ != nullptr) {
        type_node = type_node->parent_;
    }

    return type_node;
}

void TypeVariable::set_type(TypeVariable* new_type) {
    // auto old_parent = parent_;
    parent_ = new_type;

    // TODO: for this to work, need to pass in original type_var,
    //       not the resolved one, making sure to respect
    //       environment boundaries
    // // flatten to minimize depth of tree
    // while (old_parent != nullptr) {
    //     auto next_parent = old_parent->parent_;
    //     old_parent->parent_ = new_type;
    //     old_parent = next_parent;
    // }
}

// For generic function type support:
//   get fresh variables in current environment to allow for
//   substitution of free variables
void get_fresh_variable(TypeVariable* type_var) {
    // find root for type variable
    type_var = type_var->get_root();
    if (type_var->type_operator_ != nullptr) {
        for (auto &type : type_var->type_operator_->types_) {
            auto type_root = type->get_root();
            std::string type_name = type_root->type_name_;
            if (type_name != "") {
                // type name may already exist in environment,
                //  but we want a fresh one
                s_type_env[type_name] = new TypeVariable();
            }
        }
    }
}

TypeVariable* resolve_variable(TypeVariable* type_var,
                               bool update_environment) {
    // find root for type variable
    type_var = type_var->get_root();
    // check if type is in environment
    std::string type_name = type_var->type_name_;
    if (type_name != "") {
        auto inst_type = s_type_env.find(type_name);
        if (inst_type != s_type_env.end()) {
            // use type var from environment
            type_var = inst_type->second->get_root();
        }
        else if (update_environment) {
            // allocate fresh variable and update environment
            type_var = new TypeVariable();
            s_type_env[type_name] = type_var;
        }
    }
    return type_var;
}

// Flattens a potentially long chain of variables
TypeVariable* _flatten_variable(TypeVariable* type_var, TypeVariableSet &occurs,
                                std::map<TypeVariable*,TypeVariable*> &remap) {
    auto variant_name = type_var->variant_name_;
    TypeVariable* result = new TypeVariable();
    type_var = resolve_variable(type_var);
    occurs.insert(type_var);
    remap[type_var] = result;
    if (type_var->type_operator_ != nullptr) {
        std::vector<TypeVariable*> flattened_types;
        for (auto &type : type_var->type_operator_->types_) {
            auto type_root = resolve_variable(type);
            if (occurs.count(type_root) > 0) {
                flattened_types.push_back(remap[type_root]);
            }
            else {
                flattened_types.push_back(_flatten_variable(type, occurs,
                                                            remap));
            }
        }
        result->type_operator_ =
            new TypeOperator(type_var->type_operator_->type_constructor_,
                             flattened_types);
    }
    else {
        result->type_name_ = type_var->type_name_;
    }
    result->variant_name_ = variant_name;
    return result;
}

TypeVariable* flatten_variable(TypeVariable* type_var) {
    std::set<TypeVariable*> occurs;
    std::map<TypeVariable*,TypeVariable*> remap;
    return _flatten_variable(type_var, occurs, remap);
}

bool sum_type_matches(TypeOperator* variant, TypeOperator* constructor) {
    for (auto &con : variant->types_) {
        if (con->type_operator_
            && con->type_operator_->type_constructor_
               == constructor->type_constructor_) {
            return type_operators_match(con->type_operator_, constructor);
        }
    }
    return false;
}

bool _type_operators_match(TypeOperator* lhs, TypeOperator* rhs,
                           std::set<TypeOperator*> &occurs) {
    if (lhs == rhs) {
        return true;
    }

    if (lhs && occurs.count(lhs) > 0) {
        if (rhs && occurs.count(rhs) > 0) {
            return true;
        }
    }
    occurs.insert(lhs);
    occurs.insert(rhs);

    if (lhs->type_constructor_ != rhs->type_constructor_) {
        if (lhs->type_constructor_ == " | ") {
            return sum_type_matches(lhs, rhs);
        }
        else if (rhs->type_constructor_ == " | ") {
            return sum_type_matches(rhs, lhs);
        }
        else if (auto variant =
                    get_type_from_constructor(lhs->type_constructor_)) {
            variant = resolve_variable(variant);
            if (!variant->type_operator_) {
                return false;
            }
            return sum_type_matches(variant->type_operator_, rhs);
        }
        return false;
    }
    if (lhs->types_.size() != rhs->types_.size()) {
        return false;
    }

    for (size_t i = 0; i < lhs->types_.size(); ++i) {
        // auto lhtype = lhs->types_[i]->get_root();
        // auto rhtype = rhs->types_[i]->get_root();
        auto lhtype = resolve_variable(lhs->types_[i], false);
        auto rhtype = resolve_variable(rhs->types_[i], false);
        if (lhtype->type_operator_ != nullptr
            && rhtype->type_operator_ != nullptr) {
            return _type_operators_match(lhtype->type_operator_,
                                        rhtype->type_operator_, occurs);
        }
        if (lhtype != rhtype) {
            return false;
        }
    }
    return true;
}

bool type_operators_match(TypeOperator* lhs, TypeOperator* rhs) {
    std::set<TypeOperator*> occurs;
    return _type_operators_match(lhs, rhs, occurs);
}

void unify_type_operators(TypeOperator* lhs, TypeOperator* rhs,
                          std::set<TypeOperator*> &occurs);

void unify_sum_type(TypeOperator* variant, TypeOperator* constructor,
                    std::set<TypeOperator*> &occurs) {
    for (auto &con : variant->types_) {
        if (con->type_operator_
            && con->type_operator_->type_constructor_
               == constructor->type_constructor_) {
            unify_type_operators(con->type_operator_, constructor, occurs);
            return;
        }
    }
}

void _unify(TypeVariable* lhs, TypeVariable* rhs,
            std::set<TypeOperator*> &occurs);

void unify_type_operators(TypeOperator* lhs, TypeOperator* rhs,
                          std::set<TypeOperator*> &occurs) {
    if (!lhs || !rhs) {
        return;
    }

    if (lhs->type_constructor_ != rhs->type_constructor_) {
        if (lhs->type_constructor_ == " | ") {
            unify_sum_type(lhs, rhs, occurs);
        }
        else if (rhs->type_constructor_ == " | ") {
            unify_sum_type(rhs, lhs, occurs);
        }
        return;
    }

    if (lhs->types_.size() != rhs->types_.size()) {
        bon::logger.error("type error",
                          "attempting to unify types of different shape");
        return;
    }

    for (size_t i = 0; i < lhs->types_.size(); ++i) {
        _unify(lhs->types_[i], rhs->types_[i], occurs);
    }
}

// union-find for unifying types during type inference
void _unify(TypeVariable* lhs, TypeVariable* rhs,
            std::set<TypeOperator*> &occurs) {
    auto lhs_type = resolve_variable(lhs);
    auto rhs_type = resolve_variable(rhs);

    if (rhs_type == lhs_type) {
        return;
    }

    if (lhs_type->type_operator_
        && occurs.count(lhs_type->type_operator_) > 0) {
        if (rhs_type->type_operator_
            && occurs.count(rhs_type->type_operator_) > 0) {
            return;
        }
        else {
            occurs.insert(rhs_type->type_operator_);
        }
    }
    else if (lhs_type->type_operator_) {
        occurs.insert(lhs_type->type_operator_);
    }

    unify_type_operators(lhs_type->type_operator_, rhs_type->type_operator_,
                         occurs);

    // check for type mismatch
    bool both_typed = lhs_type->type_operator_ != nullptr
                      && rhs_type->type_operator_ != nullptr;
    bool mismatch = both_typed
                    && !type_operators_match(lhs_type->type_operator_,
                                             rhs_type->type_operator_);
    if (mismatch) {
        auto mismatch_str = lhs_type->get_name() + " != "
                                                 + rhs_type->get_name();
        // dump_environment();
        bon::logger.error("type mismatch", mismatch_str);
        return;
    }

    if (both_typed) {
        return;
    }

    // if one variable has concrete type, set it as root
    if (rhs_type->type_operator_ != nullptr) {
        lhs_type->set_type(rhs_type);
    }
    else if (lhs_type->type_operator_ != nullptr) {
        rhs_type->set_type(lhs_type);
    }
    else {
        // TODO: use rank to choose order instead
        lhs_type->set_type(rhs_type);
    }
}

void unify(TypeVariable* lhs, TypeVariable* rhs) {
    std::set<TypeOperator*> occurs;
    _unify(lhs, rhs, occurs);
}

bool is_boxed_type(TypeVariable* type_var) {
    type_var = resolve_variable(type_var);
    if (type_var->type_operator_ == nullptr) {
        return false;
    }
    auto op = type_var->type_operator_->type_constructor_;
    if (op == " * " || op == " | " || op == " -> " || isupper(op[0])) {
        return true;
    }

    return false;
}

bool _is_concrete_type(TypeVariable* type_var,
                       std::set<TypeOperator*> &occurs) {
    type_var = resolve_variable(type_var);
    if (type_var->type_operator_ == nullptr) {
        return false;
    }

    if (occurs.count(type_var->type_operator_) > 0) {
        return true;
    }

    occurs.insert(type_var->type_operator_);

    auto op = type_var->type_operator_->type_constructor_;
    for (auto& type : type_var->type_operator_->types_) {
        if (!_is_concrete_type(type, occurs)) {
            return false;
        }
    }

    return true;
}

bool is_concrete_type(TypeVariable* type_var) {
    std::set<TypeOperator*> occurs;
    return _is_concrete_type(type_var, occurs);
}

TypeVariable* build_function_type(std::vector<TypeVariable*> &param_types,
                                  TypeVariable* ret_type) {
    TypeVariable* in_types = nullptr;
    if (param_types.size() > 1) {
        in_types = new TypeVariable(new TypeOperator(" * ", param_types));
    }
    else if (param_types.size() == 1) {
        in_types = new TypeVariable();
        unify(in_types, param_types[0]);
    }
    else {
        in_types = UnitType;
        // in_types = new TypeVariable();
    }
    auto out_type = ret_type ? ret_type : new TypeVariable();
    std::vector<TypeVariable*> func_types(2);
    func_types[0] = in_types;
    func_types[1] = out_type;
    auto func_type = new TypeVariable(new TypeOperator(" -> ", func_types));

    return func_type;
}

TypeVariable* build_tuple_type(std::vector<TypeVariable*> &param_types) {
    if (param_types.size() > 1) {
        return new TypeVariable(new TypeOperator(" * ", param_types));
    }
    else if (param_types.size() == 1) {
        return param_types[0];
    }
    return nullptr;
}

TypeVariable* build_variant_type(TypeVariable* v_type, TypeMap &variant_types,
                                 IndexMap &fields) {
    std::vector<TypeVariable*> types;
    for (auto &pair : variant_types) {
        if (s_type_constructors.find(pair.first) != s_type_constructors.end()) {
            bon::logger.error("type error",
                              "type constructor already exists");
        }
        std::vector<TypeVariable*> constr_type;
        if (pair.second) {
            constr_type.push_back(pair.second);
        }
        auto tcon_var = new TypeVariable(new TypeOperator(pair.first,
                                                          constr_type));
        // if there is a single constructor, just return it as the type.
        // this allows us to directly access fields of the constructed
        // object without having to pattern match
        if (variant_types.size() == 1) {
            unify(v_type, tcon_var);
            s_type_constructors[pair.first] = v_type;
            s_constructor_values[pair.first] = 0;
            s_constructor_field_indices[pair.first] = fields;
            return tcon_var;
        }
        types.push_back(tcon_var);
    }
    auto var_type = new TypeVariable(new TypeOperator(" | ", types));
    uint32_t tcon_idx = 0;
    for (auto &pair : variant_types) {
        s_type_constructors[pair.first] = v_type;
        s_constructor_values[pair.first] = tcon_idx;
        ++tcon_idx;
    }
    unify(v_type, var_type);
    return var_type;
}

uint32_t get_constructor_value(std::string constructor) {
    if (s_constructor_values.find(constructor) == s_constructor_values.end()) {
        bon::logger.error("error", "unknown constructor");
    }
    return s_constructor_values[constructor];
}

uint32_t get_constructor_field_index(std::string constructor,
                                     std::string field) {
    if (s_constructor_field_indices.find(constructor)
        == s_constructor_field_indices.end()) {
        bon::logger.error("error", "unknown constructor");
    }
    if (s_constructor_field_indices[constructor].find(field)
        == s_constructor_field_indices[constructor].end()) {
            bon::logger.error("error", "unknown constructor field");
    }
    return s_constructor_field_indices[constructor][field];
}

TypeVariable* build_from_type_constructor(
                                      std::string constructor,
                                      std::vector<TypeVariable*> &param_types) {
    return new TypeVariable(new TypeOperator(constructor, param_types));
}

TypeVariable* get_type_from_constructor(std::string constructor) {
    return s_type_constructors[constructor];
}

std::string get_constructor_from_type(TypeVariable* type) {
    type = resolve_variable(type);
    if (type->type_operator_) {
        return type->type_operator_->type_constructor_;
    }
    return "";
}

TypeVariable* get_function_return_type(TypeVariable* func_type) {
    auto root = resolve_variable(func_type);
    if (root && root->type_operator_) {
        return resolve_variable(root->type_operator_->types_[1]);
    }
    return nullptr;
}

std::vector<TypeVariable*> get_function_arg_types(TypeVariable* func_type) {
    auto root = resolve_variable(func_type);
    if (root && root->type_operator_) {
        if (root->type_operator_->type_constructor_ == " | "
            || isupper(root->type_operator_->type_constructor_[0])) {
            // don't recurse on variants or constructors
            std::vector<TypeVariable*> result;
            result.push_back(root);
            return result;
        }
        auto args = resolve_variable(root->type_operator_->types_[0]);
        if (args->type_operator_->type_constructor_ == " | "
            || isupper(args->type_operator_->type_constructor_[0])) {
            // don't recurse on variants or constructors
            std::vector<TypeVariable*> result;
            result.push_back(args);
            return result;
        }
        if (args && args->type_operator_
                 && args->type_operator_->types_.size() > 0) {
            return args->type_operator_->types_;
        }
        else if (args && args->type_operator_ && args != UnitType) {
            std::vector<TypeVariable*> result;
            result.push_back(args);
            return result;
        }
    }
    return std::vector<TypeVariable*>();
}

TypeVariable* type_variable_from_identifier(std::string type_name) {
    if (type_name == "int") {
        return IntType;
    }
    else if (type_name == "float") {
        return FloatType;
    }
    else if (type_name == "string") {
        return StringType;
    }
    else if (type_name == "bool") {
        return BoolType;
    }
    else if (type_name == "()") {
        return UnitType;
    }
    else if (s_type_registry.find(type_name) != s_type_registry.end()) {
        return s_type_registry[type_name];
    }
    else if (s_typeclass_env.find(type_name) != s_typeclass_env.end()) {
        return s_typeclass_env[type_name];
    }
    std::ostringstream msg;
    bon::logger.error("type error", msg << "unknown type " << type_name
                                        << " used in type annotation" );
    return nullptr;
}

} // namespace bon
