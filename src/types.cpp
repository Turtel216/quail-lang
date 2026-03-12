#include "../include/types.hpp"
#include "../include/parsed_type.hpp"
#include "error.hpp"
#include <algorithm>
#include <memory>

namespace ff {
namespace sem {

std::string TypeManager::newTypeName() noexcept {
  int temp = this->lastId++;
  std::string str = "";

  while (temp != -1) {
    str += (char)('a' + (temp % 26));
    temp = temp / 26 - 1;
  }

  std::reverse(str.begin(), str.end());
  return str;
}

std::shared_ptr<Type> TypeManager::substitute(
    const std::map<std::string, std::shared_ptr<Type>> &subst,
    const std::shared_ptr<Type> &t) {
  std::shared_ptr<Type> temp = t;
  while (TypeVar *var = dynamic_cast<TypeVar *>(temp.get())) {
    auto subst_it = subst.find(var->getName());
    if (subst_it != subst.end())
      return subst_it->second;
    auto var_it = types.find(var->getName());
    if (var_it == types.end())
      return t;
    temp = var_it->second;
  }

  if (TypeArr *arr = dynamic_cast<TypeArr *>(temp.get())) {
    auto leftResult = substitute(subst, arr->getLeft());
    auto rightResult = substitute(subst, arr->getRight());

    if (leftResult == arr->getLeft() && rightResult == arr->getRight())
      return t;

    return std::shared_ptr<Type>(new TypeArr(leftResult, rightResult));
  } else if (TypeApp *app = dynamic_cast<TypeApp *>(temp.get())) {
    auto constructorResult = substitute(subst, app->constructor);
    bool argChanged = false;
    std::vector<std::shared_ptr<Type>> newArgs;
    for (auto &arg : app->arguments) {
      auto argResult = substitute(subst, arg);
      argChanged |= argResult != arg;
      newArgs.push_back(std::move(argResult));
    }

    if (constructorResult == app->constructor && !argChanged)
      return t;

    TypeApp *newApp = new TypeApp(std::move(constructorResult));
    std::swap(newApp->arguments, newArgs);
    return std::shared_ptr<Type>(newApp);
  }
  return t;
}

std::shared_ptr<Type> TypeManager::newType() noexcept {
  return std::shared_ptr<Type>(new TypeVar(newTypeName()));
}

std::shared_ptr<Type> TypeManager::newArrowType() noexcept {
  return std::shared_ptr<Type>(new TypeArr(newType(), newType()));
}

std::shared_ptr<Type> TypeManager::resolve(std::shared_ptr<Type> t,
                                           TypeVar *&var) const {
  TypeVar *cast;

  var = nullptr;
  while ((cast = dynamic_cast<TypeVar *>(t.get()))) {
    auto it = types.find(cast->getName());

    if (it == types.end()) {
      var = cast;
      break;
    }
    t = it->second;
  }

  return t;
}

void TypeManager::unify(std::shared_ptr<Type> l, std::shared_ptr<Type> r) {
  TypeVar *lvar, *rvar;
  TypeArr *larr, *rarr;
  TypeBase *lid, *rid;
  TypeApp *lapp, *rapp;

  l = resolve(l, lvar);
  r = resolve(r, rvar);

  if (lvar) {
    bind(lvar->getName(), r);
    return;
  } else if (rvar) {
    bind(rvar->getName(), l);
    return;
  } else if ((larr = dynamic_cast<TypeArr *>(l.get())) &&
             (rarr = dynamic_cast<TypeArr *>(r.get()))) {
    unify(larr->getLeft(), rarr->getLeft());
    unify(larr->getRight(), rarr->getRight());
    return;
  } else if ((lid = dynamic_cast<TypeBase *>(l.get())) &&
             (rid = dynamic_cast<TypeBase *>(r.get()))) {
    if (lid->getName() == rid->getName() && lid->getArity() == rid->getArity())
      return;
  } else if ((lapp = dynamic_cast<TypeApp *>(l.get())) &&
             (rapp = dynamic_cast<TypeApp *>(r.get()))) {
    unify(lapp->constructor, rapp->constructor);
    auto left_it = lapp->arguments.begin();
    auto right_it = rapp->arguments.begin();
    while (left_it != lapp->arguments.end() &&
           right_it != rapp->arguments.end()) {
      unify(*left_it, *right_it);
      left_it++, right_it++;
    }
    return;
  }

  throw ff::UnificationError(l, r);
}

void TypeManager::bind(const std::string &s, std::shared_ptr<Type> t) {
  TypeVar *other = dynamic_cast<TypeVar *>(t.get());

  if (other && other->getName() == s)
    return;
  types[s] = t;
}

std::shared_ptr<Type> TypeScheme::instantiate(TypeManager &mgr) const {
  if (forall.size() == 0)
    return monotype;

  std::map<std::string, std::shared_ptr<Type>> subst;
  for (auto &var : forall) {
    subst[var] = mgr.newType();
  }

  return mgr.substitute(subst, monotype);
}

void TypeScheme::print(const TypeManager &mgr, std::ostream &to) const {
  if (forall.size() != 0) {
    to << "forall ";
    for (auto &var : forall) {
      to << var << " ";
    }
    to << ". ";
  }
  monotype->print(mgr, to);
}

void TypeVar::print(const TypeManager &mgr, std::ostream &to) const {
  auto it = mgr.types.find(this->name);
  if (it != mgr.types.end()) {
    it->second->print(mgr, to);
  } else {
    to << this->name;
  }
}

void TypeBase::print(const TypeManager &mgr, std::ostream &to) const {
  to << this->name;
}

void TypeArr::print(const TypeManager &mgr, std::ostream &to) const {
  left->print(mgr, to);
  to << " -> (";
  right->print(mgr, to);
  to << ")";
}

void TypeApp::print(const TypeManager &mgr, std::ostream &to) const {
  constructor->print(mgr, to);
  to << "* ";
  for (auto &arg : arguments) {
    to << " ";
    arg->print(mgr, to);
  }
}

void TypeManager::findFree(const std::shared_ptr<Type> &t,
                           std::set<std::string> &into) const {
  TypeVar *var;
  auto resolved = resolve(t, var);

  if (var) {
    into.insert(var->getName());
  } else if (TypeArr *arr = dynamic_cast<TypeArr *>(resolved.get())) {
    findFree(arr->getLeft(), into);
    findFree(arr->getRight(), into);
  } else if (TypeApp *app = dynamic_cast<TypeApp *>(resolved.get())) {
    findFree(app->constructor, into);
    for (auto &arg : app->arguments)
      findFree(arg, into);
  }
}

std::shared_ptr<Type> ParsedTypeApp::toType(const std::set<std::string> &vars,
                                            const TypeContext &typeCtx) const {
  auto parentType = typeCtx.lookupType(name);
  if (parentType == nullptr)
    throw 0;

  TypeBase *baseType;
  if (!(baseType = dynamic_cast<TypeBase *>(parentType.get())))
    throw 0;

  if (baseType->getArity() != arguments.size())
    throw 0;

  TypeApp *newApp = new TypeApp(std::move(parentType));
  std::shared_ptr<Type> toReturn(newApp);
  for (auto &arg : arguments) {
    newApp->arguments.push_back(arg->toType(vars, typeCtx));
  }

  return toReturn;
}

std::shared_ptr<Type> ParsedTypeVar::toType(const std::set<std::string> &vars,
                                            const TypeContext &typeCtx) const {
  if (vars.find(var) == vars.end())
    throw 0;

  return std::shared_ptr<Type>(new TypeVar(var));
}

std::shared_ptr<Type> ParsedTypeArr::toType(const std::set<std::string> &vars,
                                            const TypeContext &typeCtx) const {
  auto new_left = left->toType(vars, typeCtx);
  auto new_right = right->toType(vars, typeCtx);

  return std::shared_ptr<Type>(
      new TypeArr(std::move(new_left), std::move(new_right)));
}

} // namespace sem
} // namespace ff
