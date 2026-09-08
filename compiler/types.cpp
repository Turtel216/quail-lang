#include "types.hpp"
#include "parsed_type.hpp"
#include "error.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <sstream>
#include <utility>

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

  /* Quoted so a fresh variable can never collide with one the program wrote
   * itself: the lexer cannot produce an identifier containing a quote, and
   * both kinds are looked up by name in the same substitution map. */
  return "'" + str;
}

std::shared_ptr<Type> TypeManager::substitute(
    const std::map<std::string, std::shared_ptr<Type>> &subst,
    const std::shared_ptr<Type> &t) {
  std::shared_ptr<Type> temp = t;
  while (TypeVar *var = dynamic_cast<TypeVar *>(temp.get())) {
    auto substIt = subst.find(var->getName());
    if (substIt != subst.end())
      return substIt->second;
    auto varIt = types.find(var->getName());
    if (varIt == types.end())
      return t;
    temp = varIt->second;
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

void TypeManager::unify(std::shared_ptr<Type> l, std::shared_ptr<Type> r,
                        const std::optional<yy::location> &loc) {
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
    unify(larr->getLeft(), rarr->getLeft(), loc);
    unify(larr->getRight(), rarr->getRight(), loc);
    return;
  } else if ((lid = dynamic_cast<TypeBase *>(l.get())) &&
             (rid = dynamic_cast<TypeBase *>(r.get()))) {
    if (lid->getName() == rid->getName() && lid->getArity() == rid->getArity())
      return;
  } else if ((lapp = dynamic_cast<TypeApp *>(l.get())) &&
             (rapp = dynamic_cast<TypeApp *>(r.get()))) {
    unify(lapp->constructor, rapp->constructor, loc);
    auto leftIt = lapp->arguments.begin();
    auto rightIt = rapp->arguments.begin();
    while (leftIt != lapp->arguments.end() &&
           rightIt != rapp->arguments.end()) {
      unify(*leftIt, *rightIt, loc);
      leftIt++, rightIt++;
    }
    return;
  }

  throw ff::UnificationError(l, r, loc);
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

void TypeBase::print(const TypeManager &, std::ostream &to) const {
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
                                            const TypeContext &typeCtx,
                                            const yy::location &loc) const {
  auto parentType = typeCtx.lookupType(name);
  if (parentType == nullptr)
    throw ff::TypeError("unknown type " + name, loc);

  /* Only data types and the built-in bases are ever bound as type names. */
  TypeBase *baseType = dynamic_cast<TypeBase *>(parentType.get());
  assert(baseType != nullptr);

  if (std::cmp_not_equal(baseType->getArity(), arguments.size())) {
    std::ostringstream errorStream;
    errorStream << "invalid application of type " << name << " ("
                << baseType->getArity() << " argument(s) expected, but "
                << arguments.size() << " provided)";

    throw ff::TypeError(errorStream.str(), loc);
  }

  TypeApp *newApp = new TypeApp(std::move(parentType));
  std::shared_ptr<Type> toReturn(newApp);
  for (auto &arg : arguments) {
    newApp->arguments.push_back(arg->toType(vars, typeCtx, loc));
  }

  return toReturn;
}

void ParsedTypeApp::collectVariables(std::set<std::string> &into) const {
  for (auto &arg : arguments)
    arg->collectVariables(into);
}

std::shared_ptr<Type> ParsedTypeVar::toType(const std::set<std::string> &vars,
                                            const TypeContext &,
                                            const yy::location &loc) const {
  if (vars.find(var) == vars.end())
    throw ff::TypeError("unbound type variable " + var, loc);

  return std::shared_ptr<Type>(new TypeVar(var));
}

void ParsedTypeVar::collectVariables(std::set<std::string> &into) const {
  into.insert(var);
}

std::shared_ptr<Type> ParsedTypeArr::toType(const std::set<std::string> &vars,
                                            const TypeContext &typeCtx,
                                            const yy::location &loc) const {
  auto newLeft = left->toType(vars, typeCtx, loc);
  auto newRight = right->toType(vars, typeCtx, loc);

  return std::shared_ptr<Type>(
      new TypeArr(std::move(newLeft), std::move(newRight)));
}

void ParsedTypeArr::collectVariables(std::set<std::string> &into) const {
  left->collectVariables(into);
  right->collectVariables(into);
}

/* The printers below write a type back out as source. Everything compound is
 * parenthesized: the parentheses do not survive parsing, so printing what was
 * printed once yields the same text again. */

void ParsedTypeApp::printSource(std::ostream &to) const {
  if (arguments.empty()) {
    to << name;
    return;
  }

  to << "(" << name;
  for (auto &argument : arguments) {
    to << " ";
    argument->printSource(to);
  }
  to << ")";
}

void ParsedTypeVar::printSource(std::ostream &to) const { to << var; }

void ParsedTypeArr::printSource(std::ostream &to) const {
  to << "(";
  left->printSource(to);
  to << " -> ";
  right->printSource(to);
  to << ")";
}

void ParsedPred::collectVariables(std::set<std::string> &into) const {
  for (auto &argument : arguments)
    argument->collectVariables(into);
}

void ParsedPred::printSource(std::ostream &to) const {
  to << className;
  for (auto &argument : arguments) {
    to << " ";
    argument->printSource(to);
  }
}

/* A context is always printed in its parenthesized form, so that one
 * predicate and several are written the same way. Both spellings parse to
 * the same list, so a program that wrote the bare form still round trips. */
void printContextSource(const ParsedContext &context, std::ostream &to) {
  if (context.empty())
    return;

  to << "(";
  for (auto it = context.begin(); it != context.end(); it++) {
    if (it != context.begin())
      to << ", ";
    (*it)->printSource(to);
  }
  to << ") => ";
}

} // namespace sem
} // namespace ff
