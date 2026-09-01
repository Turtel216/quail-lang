#pragma once

#include <location.hh>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace ff {
namespace sem {

class TypeManager;

class Type {
public:
  virtual ~Type() = default;

  virtual void print(const TypeManager &mgr, std::ostream &to) const = 0;
  ;
};

class TypeVar : public Type {
private:
  std::string name;

public:
  TypeVar(std::string n) : name(std::move(n)) {}
  std::string getName() const noexcept { return this->name; }

  void print(const TypeManager &mgr, std::ostream &to) const override;
};

class TypeBase : public Type {
private:
  std::string name;
  int32_t arity;

public:
  TypeBase(std::string n, int32_t _arity = 0)
      : name(std::move(n)), arity(_arity) {}

  std::string getName() const noexcept { return this->name; }

  void print(const TypeManager &mgr, std::ostream &to) const override;
  inline int32_t getArity() const noexcept { return this->arity; }
};

class TypeData : public TypeBase {
public:
  struct constructor {
    int tag;
  };
  std::map<std::string, constructor> constructors;

  TypeData(std::string name, int32_t _arity = 0)
      : TypeBase(std::move(name), _arity) {}
};

class TypeArr : public Type {
private:
  std::shared_ptr<Type> left;
  std::shared_ptr<Type> right;

public:
  TypeArr(std::shared_ptr<Type> l, std::shared_ptr<Type> r)
      : left(std::move(l)), right(std::move(r)) {}

  void print(const TypeManager &mgr, std::ostream &to) const override;
  inline const std::shared_ptr<Type> &getLeft() const noexcept {
    return this->left;
  }
  inline const std::shared_ptr<Type> &getRight() const noexcept {
    return this->right;
  }
};

class TypeApp : public Type {
public:
  std::shared_ptr<Type> constructor;
  std::vector<std::shared_ptr<Type>> arguments;

  TypeApp(std::shared_ptr<Type> _constructor)
      : constructor(std::move(_constructor)) {}

  void print(const TypeManager &mgr, std::ostream &to) const override;
};

class TypeManager {
private:
  int lastId = 0;

public:
  std::map<std::string, std::shared_ptr<Type>> types;

  std::string newTypeName() noexcept;
  std::shared_ptr<Type> newType() noexcept;
  std::shared_ptr<Type> newArrowType() noexcept;

  /* `loc` is the expression whose typing forced this unification; it is
   * carried down the recursion so a mismatch deep inside a type can still
   * point at the code that caused it. */
  void unify(std::shared_ptr<Type> l, std::shared_ptr<Type> r,
             const std::optional<yy::location> &loc = std::nullopt);
  std::shared_ptr<Type> resolve(std::shared_ptr<Type> t, TypeVar *&var) const;
  void bind(const std::string &s, std::shared_ptr<Type> t);
  void findFree(const std::shared_ptr<Type> &t,
                std::set<std::string> &into) const;

  std::shared_ptr<Type>
  substitute(const std::map<std::string, std::shared_ptr<Type>> &subst,
             const std::shared_ptr<Type> &t);

  inline int getLastId() const noexcept { return this->lastId; }
};

class TypeScheme {
public:
  std::vector<std::string> forall;
  std::shared_ptr<Type> monotype;

  TypeScheme(std::shared_ptr<Type> t) : forall(), monotype(std::move(t)) {}

  void print(const TypeManager &mgr, std::ostream &to) const;
  std::shared_ptr<Type> instantiate(TypeManager &mgr) const;
};
} // namespace sem
} // namespace ff
