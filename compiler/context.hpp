#pragma once

#include "types.hpp"
#include <location.hh>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace ff {
namespace sem {

/* Whether a name survives lambda lifting as a global function, or lives in a
 * stack slot and so must be captured by anything nested that refers to it. */
enum class Visibility { Global, Local };

class Variable {
public:
  std::shared_ptr<TypeScheme> scheme;
  Visibility visibility;
  std::optional<std::string> mangledName;

  /* Where each use of this name keeps the evidence it will be applied to.
   *
   * A binding is monomorphic while its own group is being solved, so a use
   * from inside that group takes on no constraints and asks for no evidence.
   * What it is really doing is passing along the dictionaries the group was
   * handed, which is only known once the group has been generalized. These
   * are the uses that have to be told so.
   *
   * The vectors are members of the expressions that hold them, which outlive
   * inference. */
  std::vector<std::vector<std::shared_ptr<EvidenceSlot>> *> uses;

  Variable(std::shared_ptr<TypeScheme> s, Visibility v, std::optional<std::string> name)
      : scheme(std::move(s)), visibility(v), mangledName(std::move(name)) {}

  Variable() : Variable(nullptr, Visibility::Local, std::nullopt) {}
};

class TypeContext {
private:
  std::shared_ptr<TypeContext> parent; // link to next node
  std::map<std::string, std::shared_ptr<Variable>> names;
  std::map<std::string, std::shared_ptr<Type>> typeNames;

public:
  TypeContext(std::shared_ptr<TypeContext> p) : parent(std::move(p)) {}
  TypeContext() : TypeContext(nullptr) {}

  std::shared_ptr<Variable> lookup(const std::string &name) const;
  void bind(const std::string &name, std::shared_ptr<Type> t,
            Visibility visibility = Visibility::Local);
  void bind(const std::string &name, std::shared_ptr<TypeScheme> t,
            Visibility visibility = Visibility::Local);
  std::shared_ptr<Type> lookupType(const std::string &name) const;
  void bindType(const std::string &typeName, std::shared_ptr<Type> t,
                const yy::location &loc = yy::location());
  void setMangledName(const std::string& name, const std::string& mangled);
  const std::string& getMangledName(const std::string& name) const;

  /* Type variables free in the surrounding bindings, skipping `except`.
   * A binding may only be generalized over the variables this does not
   * report, or a let would hand out a polymorphic type for something the
   * enclosing scope has already constrained. */
  void findFree(TypeManager &mgr, const std::set<std::string> &except,
                std::set<std::string> &into) const;

  inline const std::map<std::string, std::shared_ptr<Variable>>
  getNames() const noexcept {
    return this->names;
  }
};

std::shared_ptr<TypeContext> typeScope(std::shared_ptr<TypeContext> parent);

} // namespace sem
} // namespace ff
