#pragma once

#include <map>
#include <memory>

namespace ff {
namespace sem {

class Type {
public:
  virtual ~Type() = default;
};

class TypeVar : public Type {
private:
  std::string name;

public:
  TypeVar(std::string n) : name(std::move(n)) {}
  std::string getName() const noexcept { return this->name; }
};

class TypeBase : public Type {
private:
  std::string name;

public:
  TypeBase(std::string n) : name(std::move(n)) {}
  std::string getName() const noexcept { return this->name; }
};

class TypeArr : public Type {
public:
  std::unique_ptr<Type> left;
  std::unique_ptr<Type> right;

  TypeArr(std::unique_ptr<Type> l, std::unique_ptr<Type> r)
      : left(std::move(l)), right(std::move(r)) {}
};

class TypeMgr {
private:
  int lastId = 0;

public:
  std::map<std::string, std::unique_ptr<Type>> types;

  std::string new_type_name();
  std::unique_ptr<Type> new_type();
  std::unique_ptr<Type> new_arrow_type();

  void unify(std::unique_ptr<Type> l, std::unique_ptr<Type> r);
  std::unique_ptr<Type> resolve(std::unique_ptr<Type> t, TypeVar *&var);
  void bind(const std::string &s, std::unique_ptr<Type> t);

  int getLastId() const noexcept { return this->lastId; }
};
} // namespace sem
} // namespace ff
