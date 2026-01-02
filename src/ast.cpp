#include "../include/ast.hpp"

#include "ast.hpp"

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

std::shared_ptr<ff::sem::Type>
AstInt::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeEnv &env) const {
  return std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase("Int"));
}

std::shared_ptr<ff::sem::Type>
AstLid::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeEnv &env) const {
  return env.lookup(id);
}

std::shared_ptr<ff::sem::Type>
AstUid::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeEnv &env) const {
  return env.lookup(id);
}

std::shared_ptr<ff::sem::Type>
AstBinop::typecheck(ff::sem::TypeManager &mgr,
                    const ff::sem::TypeEnv &env) const {
  std::shared_ptr<ff::sem::Type> ltype = left->typecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> rtype = right->typecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> ftype = env.lookup(opName(op));
  if (!ftype)
    throw 0;

  std::shared_ptr<ff::sem::Type> return_type = mgr.newType();
  std::shared_ptr<ff::sem::Type> arrow_one =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(rtype, return_type));
  std::shared_ptr<ff::sem::Type> arrow_two =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(ltype, arrow_one));

  mgr.unify(arrow_two, ftype);
  return return_type;
}

std::shared_ptr<ff::sem::Type>
AstApp::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeEnv &env) const {
  std::shared_ptr<ff::sem::Type> ltype = left->typecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> rtype = right->typecheck(mgr, env);

  std::shared_ptr<ff::sem::Type> return_type = mgr.newType();
  std::shared_ptr<ff::sem::Type> arrow =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(rtype, return_type));
  mgr.unify(arrow, ltype);
  return return_type;
}

std::shared_ptr<ff::sem::Type>
AstCase::typecheck(ff::sem::TypeManager &mgr,
                   const ff::sem::TypeEnv &env) const {
  std::shared_ptr<ff::sem::Type> case_type = of->typecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> branch_type = mgr.newType();

  for (auto &branch : branches) {
    ff::sem::TypeEnv new_env = env.scope();
    branch->pattern->match(case_type, mgr, new_env);
    std::shared_ptr<ff::sem::Type> curr_branch_type =
        branch->expr->typecheck(mgr, new_env);
    mgr.unify(branch_type, curr_branch_type);
  }

  return branch_type;
}

void PatternVar::match(std::shared_ptr<ff::sem::Type> t,
                       ff::sem::TypeManager &mgr, ff::sem::TypeEnv &env) const {
  env.bind(var, t);
}

void PatternConstr::match(std::shared_ptr<ff::sem::Type> t,
                          ff::sem::TypeManager &mgr,
                          ff::sem::TypeEnv &env) const {
  std::shared_ptr<ff::sem::Type> constructor_type = env.lookup(constr);
  if (!constructor_type)
    throw 0;

  for (int i = 0; i < params.size(); i++) {
    ff::sem::TypeArr *arr =
        dynamic_cast<ff::sem::TypeArr *>(constructor_type.get());
    if (!arr)
      throw 0;

    env.bind(params[i], arr->left);
    constructor_type = arr->right;
  }

  mgr.unify(t, constructor_type);
  ff::sem::TypeBase *result_type =
      dynamic_cast<ff::sem::TypeBase *>(constructor_type.get());
  if (!result_type)
    throw 0;
}
