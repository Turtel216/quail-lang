#pragma once

#include "context.hpp"
#include "types.hpp"
#include <location.hh>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

class DefinitionClass;
class DefinitionDefn;
class DefinitionGroup;
class DefinitionInstance;

namespace ff {
namespace sem {

/* One method as its class declared it.
 *
 * `type` is what the signature spells out, with the class variable left
 * standing for whatever an instance turns out to be about, so that checking
 * an instance method is a matter of substituting the instance head into it. */
class MethodInfo {
public:
  std::string name;
  std::shared_ptr<Type> type;
  /* Every type variable the signature writes, the class variable included: a
   * method may quantify over more than its class does. */
  std::vector<std::string> forall;
  /* The declaration itself, which carries the default body when the class
   * wrote one, and the source of anything reported about the method. */
  const DefinitionDefn *declaration;

  MethodInfo(std::string n, std::shared_ptr<Type> t,
             std::vector<std::string> f, const DefinitionDefn *d)
      : name(std::move(n)), type(std::move(t)), forall(std::move(f)),
        declaration(d) {}

  bool hasDefault() const noexcept;
};

/* One instance, as `[Eq a] => Eq (List a)`. `forall` are the variables the
 * head and the context share, which are freshened together every time the
 * instance is matched against a wanted constraint. */
class InstanceInfo {
public:
  std::vector<std::string> forall;
  Qual<Pred> qual;
  const DefinitionInstance *declaration;

  InstanceInfo(std::vector<std::string> f, Qual<Pred> q,
               const DefinitionInstance *d)
      : forall(std::move(f)), qual(std::move(q)), declaration(d) {}
};

class ClassInfo {
public:
  std::string name;
  /* The one type variable the class abstracts over. Every superclass
   * constrains it, and every method signature mentions it. */
  std::string var;
  std::vector<Pred> supers;
  /* In declaration order, which is the order the dictionary keeps its method
   * fields in. */
  std::vector<MethodInfo> methods;
  std::vector<InstanceInfo> instances;
  const DefinitionClass *declaration;

  ClassInfo(std::string n, std::string v, const DefinitionClass *d)
      : name(std::move(n)), var(std::move(v)), declaration(d) {}

  const MethodInfo *lookupMethod(const std::string &methodName) const;
};

/* Every class the program declared and every instance of it, checked over
 * before inference begins so that nothing downstream has to ask whether a
 * class makes sense before using it. */
class ClassEnv {
private:
  std::map<std::string, std::unique_ptr<ClassInfo>> classes;
  /* Which class owns each method name. Two classes cannot share one: a use
   * of a method names the method, and would not say which class was meant. */
  std::map<std::string, std::string> methodOwners;

  ClassInfo *findMutable(const std::string &name);

  void declareClasses(const DefinitionGroup &group);
  void resolveSupers(const DefinitionGroup &group);
  void checkSuperCycles() const;
  void resolveMethods(const DefinitionGroup &group, const TypeContext &typeCtx);
  void resolveInstances(const DefinitionGroup &group,
                        const TypeContext &typeCtx);
  void checkOverlap() const;
  void checkInstanceMethods() const;

public:
  /* Build and validate the environment. Throws the first mistake it finds. */
  void build(const DefinitionGroup &group, const TypeContext &typeCtx);

  const ClassInfo *lookup(const std::string &name) const;
  /* The class a method belongs to, or null when no class declares it. */
  const std::string *methodOwner(const std::string &methodName) const;

  void print(std::ostream &to) const;
};

} // namespace sem
} // namespace ff
