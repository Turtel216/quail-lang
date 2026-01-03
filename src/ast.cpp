#include "../include/ast.hpp"

#include "ast.hpp"
#include "error.hpp"
#include <iostream>

std::string opName(binop op) {
  switch (op) {
  case PLUS:
    return "+";
  case MINUS:
    return "-";
  case TIMES:
    return "*";
  case DIVIDE:
    return "/";
  }
  throw 0;
}

void printIndent(int n, std::ostream &to) {
  while (n--)
    to << "  ";
}

// ############ Asts ############

std::shared_ptr<ff::sem::Type>
AstInt::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeContext &env) const {
  return std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase("Int"));
}

void AstInt::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INT: " << value << std::endl;
}

std::shared_ptr<ff::sem::Type>
AstLid::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeContext &env) const {
  return env.lookup(id);
}

void AstLid::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INT: " << id << std::endl;
}

std::shared_ptr<ff::sem::Type>
AstUid::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeContext &env) const {
  return env.lookup(id);
}

void AstUid::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INT: " << id << std::endl;
}

std::shared_ptr<ff::sem::Type>
AstBinop::typecheck(ff::sem::TypeManager &mgr,
                    const ff::sem::TypeContext &env) const {
  std::shared_ptr<ff::sem::Type> ltype = left->typecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> rtype = right->typecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> ftype = env.lookup(opName(op));
  if (!ftype)
    throw ff::TypeError(std::string("unknown binary operator ") + opName(op));

  std::shared_ptr<ff::sem::Type> return_type = mgr.newType();
  std::shared_ptr<ff::sem::Type> arrow_one =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(rtype, return_type));
  std::shared_ptr<ff::sem::Type> arrow_two =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(ltype, arrow_one));

  mgr.unify(arrow_two, ftype);
  return return_type;
}

void AstBinop::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "BINOP: " << opName(op) << std::endl;
  left->print(indent + 1, to);
  right->print(indent + 1, to);
}

std::shared_ptr<ff::sem::Type>
AstApp::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeContext &env) const {
  std::shared_ptr<ff::sem::Type> ltype = left->typecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> rtype = right->typecheck(mgr, env);

  std::shared_ptr<ff::sem::Type> return_type = mgr.newType();
  std::shared_ptr<ff::sem::Type> arrow =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(rtype, return_type));
  mgr.unify(arrow, ltype);
  return return_type;
}

void AstApp::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "APP:" << std::endl;
  left->print(indent + 1, to);
  right->print(indent + 1, to);
}

std::shared_ptr<ff::sem::Type>
AstCase::typecheck(ff::sem::TypeManager &mgr,
                   const ff::sem::TypeContext &env) const {
  ff::sem::TypeVar *var;
  std::shared_ptr<ff::sem::Type> case_type =
      mgr.resolve(of->typecheck(mgr, env), var);
  std::shared_ptr<ff::sem::Type> branch_type = mgr.newType();

  for (auto &branch : branches) {
    ff::sem::TypeContext new_env = env.scope();

    branch->pattern->match(case_type, mgr, new_env);
    std::shared_ptr<ff::sem::Type> curr_branch_type =
        branch->expr->typecheck(mgr, new_env);

    mgr.unify(branch_type, curr_branch_type);
  }

  case_type = mgr.resolve(case_type, var);
  if (!dynamic_cast<ff::sem::TypeBase *>(case_type.get())) {
    throw ff::TypeError("attempting case analysis of non-data type");
  }

  return branch_type;
}

void AstCase::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "CASE: " << std::endl;
  for (auto &branch : branches) {
    printIndent(indent + 1, to);
    branch->pattern->print(to);
    to << std::endl;
    branch->expr->print(indent + 2, to);
  }
}

void PatternVar::match(std::shared_ptr<ff::sem::Type> t,
                       ff::sem::TypeManager &mgr,
                       ff::sem::TypeContext &env) const {
  env.bind(var, t);
}

void PatternVar::print(std::ostream &to) const { to << var; }

void PatternConstr::match(std::shared_ptr<ff::sem::Type> t,
                          ff::sem::TypeManager &mgr,
                          ff::sem::TypeContext &env) const {
  std::shared_ptr<ff::sem::Type> constructor_type = env.lookup(constr);
  if (!constructor_type)
    throw ff::TypeError(std::string("pattern using unknown constructor ") +
                        constr);

  for (int i = 0; i < params.size(); i++) {
    ff::sem::TypeArr *arr =
        dynamic_cast<ff::sem::TypeArr *>(constructor_type.get());
    if (!arr)
      throw ff::TypeError("too many parameters in constructor pattern");

    env.bind(params[i], arr->left);
    constructor_type = arr->right;
  }

  mgr.unify(t, constructor_type);
}

void PatternConstr::print(std::ostream &to) const {
  to << constr;
  for (auto &param : params) {
    to << " " << param;
  }
}

// ############ Definitions ############

void DefinitionDefn::typeCheckFirst(ff::sem::TypeManager &mgr,
                                    ff::sem::TypeContext &env) {
  this->returnType = mgr.newType();
  std::shared_ptr<ff::sem::Type> fullType = this->returnType;

  for (auto it = this->params.rbegin(); it != this->params.rend(); it++) {
    std::shared_ptr<ff::sem::Type> paramType = mgr.newType();
    fullType = std::shared_ptr<ff::sem::Type>(
        new ff::sem::TypeArr(paramType, fullType));
    this->paramTypes.push_back(paramType);
  }

  env.bind(name, fullType);
}

void DefinitionDefn::typeCheckSecond(ff::sem::TypeManager &mgr,
                                     const ff::sem::TypeContext &env) const {
  ff::sem::TypeContext newEnv = env.scope();
  auto param_it = this->params.begin();
  auto type_it = this->paramTypes.rbegin();

  while (param_it != params.end() && type_it != this->paramTypes.rend()) {
    newEnv.bind(*param_it, *type_it);
    param_it++;
    type_it++;
  }

  std::shared_ptr<ff::sem::Type> body_type = body->typecheck(mgr, newEnv);
  mgr.unify(this->returnType, body_type);
}

void DefinitionData::typeCheckFirst(ff::sem::TypeManager &mgr,
                                    ff::sem::TypeContext &env) {
  std::shared_ptr<ff::sem::Type> return_type =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase(name));

  for (auto &constructor : constructors) {
    std::shared_ptr<ff::sem::Type> full_type = return_type;

    for (auto it = constructor->types.rbegin(); it != constructor->types.rend();
         it++) {
      std::shared_ptr<ff::sem::Type> type =
          std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase(*it));
      full_type =
          std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(type, full_type));
    }

    env.bind(constructor->name, full_type);
  }
}

void DefinitionData::typeCheckSecond(ff::sem::TypeManager &mgr,
                                     const ff::sem::TypeContext &env) const {
  // TODO ?
}
