#include "../include/enviroment.hpp"
#include <cassert>

namespace ff {
namespace ir {
int EnviromentVar::getOffset(const std::string &name) const {
  if (name == this->name)
    return 0;

  /* Only names the generator has already put in scope are ever looked up. */
  assert(this->parent != nullptr);
  return 1 + this->parent->getOffset(name);
}

bool EnviromentVar::hasVariable(const std::string &name) const {
  if (name == this->name)
    return true;

  return parent && parent->hasVariable(name);
}

int EnviromentOffset::getOffset(const std::string &name) const {
  assert(parent != nullptr);
  return offset + parent->getOffset(name);
}

bool EnviromentOffset::hasVariable(const std::string &name) const {
  if (parent)
    return parent->hasVariable(name);

  return false;
}
} // namespace ir
} // namespace ff
