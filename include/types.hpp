#pragma once

#include <map>
#include <memory>
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

public:
  TypeBase(std::string n) : name(std::move(n)) {}

  std::string getName() const noexcept { return this->name; }

  void print(const TypeManager &mgr, std::ostream &to) const override;
};

class TypeData : public TypeBase {
public:
  struct constructor {
    int tag;
  };
  std::map<std::string, constructor> constructors;

  TypeData(std::string name) : TypeBase(std::move(name)) {}
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

class TypeManager {
private:
  int lastId = 0;

public:
  std::map<std::string, std::shared_ptr<Type>> types;

  std::string newTypeName() noexcept;
  std::shared_ptr<Type> newType() noexcept;
  std::shared_ptr<Type> newArrowType() noexcept;

  void unify(std::shared_ptr<Type> l, std::shared_ptr<Type> r);
  std::shared_ptr<Type> resolve(std::shared_ptr<Type> t, TypeVar *&var) const;
  void bind(const std::string &s, std::shared_ptr<Type> t);
  void findFree(const std::shared_ptr<Type> &t,
                std::set<std::string> &into) const;

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
