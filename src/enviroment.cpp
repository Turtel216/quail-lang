#include "../include/enviroment.hpp"

namespace ff {
namespace ir {
int EnviromentVar::getOffset(const std::string &name) const {
  if (name == this->name)
    return 0;

  if (this->parent)
    return 1 + this->parent->getOffset(name);

  throw 0;
}

bool EnviromentVar::hasVariable(const std::string &name) const {
  if (name == this->name)
    return true;

  if (parent)
    return parent->hasVariable(name);

  return false;
}

int EnviromentOffset::getOffset(const std::string &name) const {
  if (parent)
    return offset + parent->getOffset(name);

  throw 0;
}

bool EnviromentOffset::hasVariable(const std::string &name) const {
  if (parent)
    return parent->hasVariable(name);

  return false;
}
} // namespace ir
} // namespace ff
