#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = rv_.get()->Execute(closure, context);
    return closure[var_];
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) : var_(var), rv_(move(rv)) {
}

VariableValue::VariableValue(const std::string& var_name) {
    var_names_.push_back(var_name);
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids) : var_names_(dotted_ids) {
}

ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    if (closure.count(var_names_[0])) {
        ObjectHolder holder = closure[var_names_[0]];
        if (var_names_.size() == 1) {
            return holder;
        } else {
            for(size_t i = 1; i < var_names_.size(); ++i) {
                runtime::ClassInstance* obj_ptr = holder.TryAs<runtime::ClassInstance>();
                if (obj_ptr) {
                    holder = obj_ptr->Fields()[var_names_[i]];
                } else {
                    throw std::runtime_error("there's no such field for object"s);
                }
            }
            return holder;
        }
    } else {
        throw std::runtime_error("there's no such var"s);
    }
    return {};
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return make_unique<Print>(make_unique<VariableValue>(name));
}

Print::Print(unique_ptr<Statement> argument){
    args_.push_back(move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args) : args_(move(args)) {
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    for(size_t i = 0; i < args_.size(); ++i) {
        auto obj_ptr = args_[i].get()->Execute(closure, context).Get();
        if (obj_ptr) {
            obj_ptr->Print(context.GetOutputStream(), context);
        } else {
            context.GetOutputStream() << "None"s;
        }
        if(i != args_.size() - 1) {
            context.GetOutputStream() << " "s;
        }
    }
    context.GetOutputStream() << "\n"s;
    return {};
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args) : object_(move(object)), method_(method), args_(move(args)) {
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    runtime::ClassInstance* obj_ptr = object_.get()->Execute(closure, context).TryAs<runtime::ClassInstance>();
    if (obj_ptr && obj_ptr->HasMethod(method_, args_.size())) {
        vector<ObjectHolder> local_args;
        for (size_t i = 0; i < args_.size(); ++i) {
            local_args.push_back(args_[i].get()->Execute(closure, context));
        }
        return obj_ptr->Call(method_, local_args, context);
    }
    return {};
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    ObjectHolder holder = argument_.get()->Execute(closure, context);
    if (bool(holder)) {
        ostringstream string_stream;
        holder.Get()->Print(string_stream, context);
        return ObjectHolder::Own(runtime::String(string_stream.str()));
    } else {
        return ObjectHolder::Own(runtime::String("None"s));
    }
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);
    optional<runtime::Number> num_res = runtime::IsObjectComparable<runtime::Number, runtime::Number>(lhs_holder, rhs_holder, plus());
    if (num_res) {
        return ObjectHolder::Own(runtime::Number(num_res.value()));
    }
    optional<runtime::String> str_res = runtime::IsObjectComparable<runtime::String, runtime::String>(lhs_holder, rhs_holder, plus());
    if (str_res) {
        return ObjectHolder::Own(runtime::String(str_res.value()));
    }
    runtime::ClassInstance* class_instance_ptr = lhs_holder.TryAs<runtime::ClassInstance>();
    if (class_instance_ptr && class_instance_ptr->HasMethod(ADD_METHOD, 1)) {
        return class_instance_ptr->Call(ADD_METHOD, {rhs_holder}, context);
    }
    throw runtime_error("No operation for this args"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);
    optional<runtime::Number> num_res = runtime::IsObjectComparable<runtime::Number, runtime::Number>(lhs_holder, rhs_holder, minus());
    if (num_res) {
        return ObjectHolder::Own(runtime::Number(num_res.value()));
    }
    throw runtime_error("No operation for this args"s);
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);
    optional<runtime::Number> num_res = runtime::IsObjectComparable<runtime::Number, runtime::Number>(lhs_holder, rhs_holder, multiplies());
    if (num_res) {
        return ObjectHolder::Own(runtime::Number(num_res.value()));
    }
    throw runtime_error("No operation for this args"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    auto rhs_holder = rhs_.get()->Execute(closure, context);
    auto obj_ptr = rhs_holder.TryAs<runtime::Number>();
    if (obj_ptr && obj_ptr->GetValue() == 0) {
        throw runtime_error("division by zero"s);
    }
    optional<runtime::Number> num_res = runtime::IsObjectComparable<runtime::Number, runtime::Number>(lhs_holder, rhs_holder, divides());
    if (num_res) {
        return ObjectHolder::Own(runtime::Number(num_res.value()));
    }
    throw runtime_error("No operation for this args"s);
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for(auto& stmt : stmts_) {
        stmt.get()->Execute(closure, context);
    }
    return {};
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    throw  statement_.get()->Execute(closure, context);
    return {};
}

ClassDefinition::ClassDefinition(ObjectHolder cls) : cls_(cls) {
}

ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
    return {};
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv) : object_(object), field_name_(field_name), rv_(move(rv)) {
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    object_.Execute(closure, context).TryAs<runtime::ClassInstance>()->Fields()[field_name_] = rv_->Execute(closure, context);
    return object_.Execute(closure, context).TryAs<runtime::ClassInstance>()->Fields()[field_name_];
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body) : condition_(move(condition)), if_body_(move(if_body)), else_body_(move(else_body)) {
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    if (runtime::IsTrue(condition_.get()->Execute(closure, context))) {
        return if_body_.get()->Execute(closure, context);
    } else {
        auto else_body_ptr = else_body_.get();
        if (else_body_ptr)
            return else_body_ptr->Execute(closure, context);
        else {
            return {};
        }
    }
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    if (runtime::IsTrue(lhs_holder)) {
        return ObjectHolder::Own(runtime::Bool(true));
    } else {
        auto rhs_holder = rhs_.get()->Execute(closure, context);
        return ObjectHolder::Own(runtime::Bool(runtime::IsTrue(rhs_holder)));
    }
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    auto lhs_holder = lhs_.get()->Execute(closure, context);
    if (!runtime::IsTrue(lhs_holder)) {
        return ObjectHolder::Own(runtime::Bool(false));
    } else {
        auto rhs_holder = rhs_.get()->Execute(closure, context);
        return ObjectHolder::Own(runtime::Bool(runtime::IsTrue(rhs_holder)));
    }
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    auto arg_holder = argument_.get()->Execute(closure, context);
    return ObjectHolder::Own(runtime::Bool(!runtime::IsTrue(arg_holder)));
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(cmp) {
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    return ObjectHolder::Own(runtime::Bool(cmp_(lhs_.get()->Execute(closure, context), rhs_.get()->Execute(closure, context), context)));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args) : class_ref_(class_), args_(move(args)) {
}

NewInstance::NewInstance(const runtime::Class& class_) : class_ref_(class_) {
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    ObjectHolder obj = runtime::ObjectHolder::Own(runtime::ClassInstance{class_ref_});
    runtime::ClassInstance* instance_ptr = obj.TryAs<runtime::ClassInstance>();
    if (instance_ptr->HasMethod(INIT_METHOD, args_.size())) {
        vector<ObjectHolder> local_args;
        for (size_t i = 0; i < args_.size(); ++i) {
            local_args.push_back(args_[i].get()->Execute(closure, context));
        }
        instance_ptr->Call(INIT_METHOD, local_args, context);
    }
    return obj;
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body) : body_(std::move(body)) {
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        body_.get()->Execute(closure, context);
    } catch (ObjectHolder& e) {
        return e;
    }
    return {};
}

}  // namespace ast
