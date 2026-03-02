#pragma once

#include <map>
#include <memory>

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
public:
  std::shared_ptr<Type> left;
  std::shared_ptr<Type> right;

  TypeArr(std::shared_ptr<Type> l, std::shared_ptr<Type> r)
      : left(std::move(l)), right(std::move(r)) {}

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

  void unify(std::shared_ptr<Type> l, std::shared_ptr<Type> r);
  std::shared_ptr<Type> resolve(std::shared_ptr<Type> t, TypeVar *&var) const;
  void bind(const std::string &s, std::shared_ptr<Type> t);

  int getLastId() const noexcept { return this->lastId; }
};
} // namespace sem
} // namespace ff
