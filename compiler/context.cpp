#include "context.hpp"
#include "error.hpp"
#include <cassert>
#include <set>

namespace ff {
namespace sem {
std::shared_ptr<Variable> TypeContext::lookup(const std::string &name) const {
  auto it = this->names.find(name);
  if (it != this->names.end())
    return it->second;

  if (this->parent)
    return this->parent->lookup(name);

  return nullptr;
}

std::shared_ptr<Type> TypeContext::lookupType(const std::string &name) const {
  auto it = typeNames.find(name);
  if (it != typeNames.end())
    return it->second;
  if (parent)
    return parent->lookupType(name);
  return nullptr;
}

void TypeContext::setMangledName(const std::string& name, const std::string & mangled) {
  auto it = names.find(name);

  // Variable must exist
  assert(it != names.end());
  // Do not mangle local variables 
  assert(it->second->visibility == Visibility::Global);

  it->second->mangledName = mangled;
}

  const std::string& TypeContext::getMangledName(const std::string& name) const {
    auto it = names.find(name);
    if(it != names.end()) {
      assert(it->second->mangledName);
      return *it->second->mangledName;
    }

    assert(parent != nullptr);
    return parent->getMangledName(name);
  }

void TypeContext::bindType(const std::string &typeName, std::shared_ptr<Type> t,
                           const yy::location &loc) {
  if (lookupType(typeName) != nullptr)
    throw ff::CompilerError("type " + typeName + " is already defined", loc);

  typeNames[typeName] = t;
}

void TypeContext::bind(const std::string &name, std::shared_ptr<Type> t,
                       Visibility visibility) {
  bind(name, std::shared_ptr<TypeScheme>(new TypeScheme(std::move(t))),
       visibility);
}

void TypeContext::bind(const std::string &name, std::shared_ptr<TypeScheme> t,
                       Visibility visibility) {
  names[name] =
      std::shared_ptr<Variable>(new Variable(std::move(t), visibility, std::nullopt));
}

std::shared_ptr<TypeContext> typeScope(std::shared_ptr<TypeContext> parent) {
  return std::shared_ptr<TypeContext>(new TypeContext(std::move(parent)));
}

void TypeContext::findFree(TypeManager &mgr,
                           const std::set<std::string> &except,
                           std::set<std::string> &into) const {
  for (auto &pair : names) {
    if (except.find(pair.first) != except.end())
      continue;

    std::set<std::string> freeVariables;
    mgr.findFree(pair.second->scheme->monotype, freeVariables);

    /* Quantified variables are replaced on every instantiation, so they
     * constrain nothing here. */
    for (auto &quantified : pair.second->scheme->forall)
      freeVariables.erase(quantified);

    into.insert(freeVariables.begin(), freeVariables.end());
  }

  if (parent)
    parent->findFree(mgr, except, into);
}

void TypeContext::generalize(const std::string &name,
                             const std::set<std::string> &except,
                             TypeManager &mgr) {
  auto namesIt = names.find(name);
  /* Nothing outside of typechecking generalizes, and a binding is only ever
   * reached once by its own group. */
  assert(namesIt != names.end());
  assert(namesIt->second->scheme->forall.size() == 0);

  std::set<std::string> boundVariables;
  findFree(mgr, except, boundVariables);

  std::set<std::string> freeVariables;
  mgr.findFree(namesIt->second->scheme->monotype, freeVariables);
  for (auto &free : freeVariables) {
    if (boundVariables.find(free) == boundVariables.end())
      namesIt->second->scheme->forall.push_back(free);
  }
}
} // namespace sem
} // namespace ff
