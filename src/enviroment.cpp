#include "../include/enviroment.hpp"

namespace ff {
namespace sem {
std::shared_ptr<Type> TypeEnv::lookup(const std::string &name) const {
  auto it = this->names.find(name);
  if (it != this->names.end())
    return it->second;

  if (this->parent)
    return this->parent->lookup(name);

  return nullptr;
}

void TypeEnv::bind(const std::string &name, std::shared_ptr<Type> t) {
  this->names[name] = t;
}

TypeEnv TypeEnv::scope() const { return TypeEnv(this); }
} // namespace sem
} // namespace ff
