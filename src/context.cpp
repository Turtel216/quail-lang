#include "../include/context.hpp"

namespace ff {
namespace sem {
std::shared_ptr<Type> TypeContext::lookup(const std::string &name) const {
  auto it = this->names.find(name);
  if (it != this->names.end())
    return it->second;

  if (this->parent)
    return this->parent->lookup(name);

  return nullptr;
}

void TypeContext::bind(const std::string &name, std::shared_ptr<Type> t) {
  this->names[name] = t;
}

TypeContext TypeContext::scope() const { return TypeContext(this); }
} // namespace sem
} // namespace ff
