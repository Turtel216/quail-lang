#include "../include/types.hpp"
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

std::shared_ptr<Type>
substitute(const TypeManager &mgr,
           const std::map<std::string, std::shared_ptr<Type>> &subst,
           const std::shared_ptr<Type> &t) {
  TypeVar *var;
  auto resolved = mgr.resolve(t, var);
  if (var) {
    auto subst_it = subst.find(var->getName());
    if (subst_it == subst.end())
      return resolved;
    return subst_it->second;
  } else if (TypeArr *arr = dynamic_cast<TypeArr *>(t.get())) {
    auto left_result = substitute(mgr, subst, arr->getLeft());
    auto right_result = substitute(mgr, subst, arr->getRight());
    if (left_result == arr->getLeft() && right_result == arr->getRight())
      return t;
    return std::shared_ptr<Type>(new TypeArr(left_result, right_result));
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
  TypeVar *lvar;
  TypeVar *rvar;
  TypeArr *larr;
  TypeArr *rarr;
  TypeBase *lid;
  TypeBase *rid;

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
    if (lid->getName() == rid->getName())
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

  return substitute(mgr, subst, monotype);
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

void TypeManager::findFree(const std::shared_ptr<Type> &t,
                           std::set<std::string> &into) const {
  TypeVar *var;
  auto resolved = resolve(t, var);

  if (var) {
    into.insert(var->getName());
  } else if (TypeArr *arr = dynamic_cast<TypeArr *>(resolved.get())) {
    findFree(arr->getLeft(), into);
    findFree(arr->getRight(), into);
  }
}

} // namespace sem
} // namespace ff
