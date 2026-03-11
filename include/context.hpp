#pragma once

#include "types.hpp"
#include <map>
#include <memory>

namespace ff {
namespace sem {

class TypeContext {
private:
  std::shared_ptr<TypeContext> parent; // link to next node
  std::map<std::string, std::shared_ptr<TypeScheme>> names;
  std::map<std::string, std::shared_ptr<Type>> typeNames;

public:
  TypeContext(std::shared_ptr<TypeContext> p) : parent(std::move(p)) {}
  TypeContext() : TypeContext(nullptr) {}

  std::shared_ptr<TypeScheme> lookup(const std::string &name) const;
  void bind(const std::string &name, std::shared_ptr<Type> t);
  std::shared_ptr<Type> lookupType(const std::string &name) const;
  void bindType(const std::string &typeName, std::shared_ptr<Type> t);
  void generalize(const std::string &name, TypeManager &mgr);

  inline const std::map<std::string, std::shared_ptr<TypeScheme>>
  getNames() const noexcept {
    return this->names;
  }
};

std::shared_ptr<TypeContext> typeScope(std::shared_ptr<TypeContext> parent);

} // namespace sem
} // namespace ff
