#pragma once
#include "types.hpp"
#include <map>
#include <memory>

namespace ff {
namespace sem {

class TypeEnv {
  std::map<std::string, std::shared_ptr<Type>> names;
  TypeEnv const *parent = nullptr; // link to next node

  TypeEnv(TypeEnv const *p) : parent(p) {}
  TypeEnv() : TypeEnv(nullptr) {}

  std::shared_ptr<Type> lookup(const std::string &name) const;
  void bind(const std::string &name, std::shared_ptr<Type> t);
  TypeEnv scope() const;
};

} // namespace sem
} // namespace ff
