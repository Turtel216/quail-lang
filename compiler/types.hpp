#pragma once

#include <location.hh>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace ff {
namespace sem {

/* The built-in list type and its constructors. The compiler registers these
 * itself instead of letting the prelude declare them, so that the [...]
 * syntax always has a type to build. */
inline constexpr const char *listTypeName = "List";
inline constexpr const char *listNilName = "Nil";
inline constexpr const char *listConsName = "Cons";
inline constexpr int listNilTag = 0;
inline constexpr int listConsTag = 1;

/* The type an if condition must have. Unlike the list, Bool is an ordinary
 * data type declared by the prelude, so `if` looks these names up instead of
 * assuming they are there. TODO: this might be stupid and needs to be updated
 * and work similar to list
 */
inline constexpr const char *boolTypeName = "Bool";
inline constexpr const char *boolTrueName = "True";
inline constexpr const char *boolFalseName = "False";

/* Function composition is built so that `f . g` always has
 * a supercombinator to build. The name is the operator itself, which no
 * program can bind, and the action is what its symbol is spelled out as. */
inline constexpr const char *composeName = ".";
inline constexpr const char *composeAction = "compose";

/* The class an ambiguous type variable has to be constrained by before the
 * defaulting rules will settle it, and the types they settle it to, in the
 * order they are tried. Named here because both inference and the class
 * environment have to agree on what counts as numeric. */
inline constexpr const char *numClassName = "Num";

class TypeManager;
class ClassEnv;

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

/* One class constraint: the class a type is claimed to be an instance of.
 *
 * The type is held rather than the name of a variable, because a wanted
 * constraint is about whatever inference has arrived at, which may be a
 * variable, an applied constructor or a function. Comparing two of these
 * therefore goes through the same resolution that unification does. */
class Pred {
public:
  std::string className;
  std::shared_ptr<Type> type;

  Pred(std::string c, std::shared_ptr<Type> t)
      : className(std::move(c)), type(std::move(t)) {}

  void print(const TypeManager &mgr, std::ostream &to) const;
};

/* A payload under the constraints it holds only under. The payload is a type
 * for a qualified type, and a predicate for an instance declaration: reading
 * `[Eq a] => Eq (List a)` as "lists of a are equatable whenever a is". */
template <typename T> class Qual {
public:
  std::vector<Pred> preds;
  T head;

  Qual(std::vector<Pred> p, T h) : preds(std::move(p)), head(std::move(h)) {}
};

/* One constraint inference has run into and not yet accounted for, and the
 * expression whose typing ran into it, so that a constraint nothing can
 * solve is reported against the code that wanted it. */
class Wanted {
public:
  Pred pred;
  yy::location loc;

  Wanted(Pred p, yy::location l) : pred(std::move(p)), loc(std::move(l)) {}
};

class TypeManager {
private:
  int lastId = 0;
  /* Constraints collected as inference runs. A binding group takes the ones
   * its own bodies wanted, solves what it can, and hands back whatever the
   * scope around it has to answer for. */
  std::vector<Wanted> wanted;
  const ClassEnv *classEnv = nullptr;

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

  void want(Pred pred, const yy::location &loc);
  /* Where the wanted set currently ends, so that everything a binding group
   * goes on to want can be taken back off in one piece. */
  std::size_t wantedMark() const noexcept;
  std::vector<Wanted> takeWantedFrom(std::size_t mark);

  inline void setClassEnv(const ClassEnv &env) noexcept {
    this->classEnv = &env;
  }
  inline const ClassEnv *getClassEnv() const noexcept { return this->classEnv; }
};

class TypeScheme {
public:
  std::vector<std::string> forall;
  /* What the scheme holds under. Empty for everything that is not
   * overloaded, which is what keeps ordinary code on the path it was on. */
  std::vector<Pred> context;
  std::shared_ptr<Type> monotype;

  TypeScheme(std::shared_ptr<Type> t)
      : forall(), context(), monotype(std::move(t)) {}

  void print(const TypeManager &mgr, std::ostream &to) const;

  /* Freshen the quantified variables. The constraints the scheme holds under
   * are freshened with them and handed to the manager as wanted, since a use
   * of an overloaded name is exactly where its constraints are taken on.
   * `loc` is the use, so that a constraint nothing solves can be reported
   * against it. */
  std::shared_ptr<Type> instantiate(TypeManager &mgr,
                                    const yy::location &loc) const;
};

/* Short, stable names for the variables of a type being written out. A type
 * printed for a person should not depend on how many variables were made
 * before it, which the compiler's own names do. */
class TypeNamer {
private:
  std::map<std::string, std::string> names;

public:
  const std::string &nameOf(const std::string &var);
};

/* Write a type the way a program would: applications spelled out, arrows to
 * the right, and variables named by `namer`. */
void printReadable(const TypeManager &mgr, const std::shared_ptr<Type> &type,
                   TypeNamer &namer, std::ostream &to);
void printReadable(const TypeManager &mgr, const Pred &pred, TypeNamer &namer,
                   std::ostream &to);
void printReadable(const TypeManager &mgr, const TypeScheme &scheme,
                   std::ostream &to);
} // namespace sem
} // namespace ff
