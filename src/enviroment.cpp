#include "../include/enviroment.hpp"
#include "error.hpp"

namespace ff {
namespace ir {
int EnviromentVar::getOffset(const std::string &name) const {
  if (name == this->name)
    return 0;

  if (this->parent)
    return 1 + this->parent->getOffset(name);

  throw ff::DebugError("EnviromentVar getOffset error");
}

bool EnviromentVar::hasVariable(const std::string &name) const {
  if (name == this->name)
    return true;

  if (parent)
    return parent->hasVariable(name);

  throw ff::DebugError("EnviromentVar hasVariable error");
}

int EnviromentOffset::getOffset(const std::string &name) const {
  if (parent)
    return offset + parent->getOffset(name);

  throw ff::DebugError("EnviromentOffset getOffset error");
}

bool EnviromentOffset::hasVariable(const std::string &name) const {
  if (parent)
    return parent->hasVariable(name);

  return false;
}
} // namespace ir
} // namespace ff
