#pragma once

#include "types.hpp"
#include <map>
#include <memory>

namespace ff {
namespace sem {

class TypeContext {
private:
  std::map<std::string, std::shared_ptr<Type>> names;
  TypeContext const *parent = nullptr; // link to next node
public:
  TypeContext(TypeContext const *p) : parent(p) {}
  TypeContext() : TypeContext(nullptr) {}

  std::shared_ptr<Type> lookup(const std::string &name) const;
  void bind(const std::string &name, std::shared_ptr<Type> t);
  TypeContext scope() const;

  inline const std::map<std::string, std::shared_ptr<Type>>
  getNames() const noexcept {
    return this->names;
  }
};

} // namespace sem
} // namespace ff
