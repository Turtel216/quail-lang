#include "../include/context.hpp"
#include <set>

namespace ff {
namespace sem {
std::shared_ptr<TypeScheme> TypeContext::lookup(const std::string &name) const {
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

void TypeContext::bindType(const std::string &typeName,
                           std::shared_ptr<Type> t) {
  if (lookupType(typeName) != nullptr)
    throw 0;

  typeNames[typeName] = t;
}

void TypeContext::bind(const std::string &name, std::shared_ptr<Type> t) {
  this->names[name] = std::shared_ptr<TypeScheme>(new TypeScheme(t));
}

void TypeContext::bind(const std::string &name, std::shared_ptr<TypeScheme> t) {
  names[name] = t;
}

std::shared_ptr<TypeContext> typeScope(std::shared_ptr<TypeContext> parent) {
  return std::shared_ptr<TypeContext>(new TypeContext(std::move(parent)));
}

void TypeContext::generalize(const std::string &name, TypeManager &mgr) {
  auto namesIt = names.find(name);
  if (namesIt == names.end())
    throw 0;
  if (namesIt->second->forall.size() > 0)
    throw 0;

  std::set<std::string> freeVariables;
  mgr.findFree(namesIt->second->monotype, freeVariables);
  for (auto &free : freeVariables) {
    namesIt->second->forall.push_back(free);
  }
}
} // namespace sem
} // namespace ff
