#pragma once

#include "context.hpp"
#include "types.hpp"
#include <location.hh>
#include <map>
#include <memory>
#include <optional>
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
  DefinitionInstance *declaration;

  InstanceInfo(std::vector<std::string> f, Qual<Pred> q,
               DefinitionInstance *d)
      : forall(std::move(f)), qual(std::move(q)), declaration(d) {}
};

/* A dictionary already in hand while solving: what it says, and how to get
 * hold of it. */
class Given {
public:
  Pred pred;
  std::shared_ptr<EvidenceTerm> evidence;

  Given(Pred p, std::shared_ptr<EvidenceTerm> e)
      : pred(std::move(p)), evidence(std::move(e)) {}
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
  std::vector<std::shared_ptr<Type>> defaults;

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

  /* Bind every method as a global, under the qualified scheme its class
   * gives it. A method is reached by name like any other function; what
   * makes it a method is the constraint its scheme carries. */
  void bindMethods(TypeContext &typeCtx) const;

  inline bool isEmpty() const noexcept { return this->classes.empty(); }

  const ClassInfo *lookup(const std::string &name) const;
  /* The class a method belongs to, or null when no class declares it. */
  const std::string *methodOwner(const std::string &methodName) const;

  void print(std::ostream &to) const;

  /* -- Entailment -------------------------------------------------------- */

  /* A constraint together with everything it already implies, which is what
   * each of its superclasses says about the same type, transitively. Holding
   * `Ord a` is holding `Eq a`, and the dictionary for the first carries the
   * dictionary for the second, so nothing has to be solved again. */
  std::vector<Pred> bySuper(TypeManager &mgr, const Pred &pred) const;

  /* The context of the one instance whose head matches `pred`, or nothing
   * when no instance does. Matching is one way: the instance head's
   * variables are what may stand for something, the constraint's are not. */
  std::optional<std::vector<Pred>> byInst(TypeManager &mgr,
                                          const Pred &pred) const;

  /* Whether `given` already answers for `wanted`, by a superclass of
   * something held or by an instance whose own context is likewise answered
   * for. */
  bool entail(TypeManager &mgr, const std::vector<Pred> &given,
              const Pred &wanted) const;

  /* Reduce a constraint to ones about type variables alone, by replacing
   * anything constructor headed with the context of the instance that
   * answers it. Throws when nothing does. */
  std::vector<Pred> toHnf(TypeManager &mgr, const Pred &pred,
                          const yy::location &loc,
                          const std::string &provenance = "") const;

  /* Drop every constraint the others already answer for. Each keeps the
   * place it came from, so what is left can still be reported against the
   * code that wanted it. */
  std::vector<Wanted> simplify(TypeManager &mgr,
                               std::vector<Wanted> wanted) const;

  /* toHnf over every wanted constraint, then simplify what comes back.
   * Anything `given` already answers for is dropped before reduction, since
   * a constraint that is held needs no instance to justify it. */
  std::vector<Wanted> reduce(TypeManager &mgr, std::vector<Wanted> wanted,
                             const std::vector<Pred> &given = {}) const;

  /* -- Defaulting -------------------------------------------------------- */

  /* The types an ambiguous variable may be settled to, in the order they are
   * tried. Set once, before inference. */
  void setDefaults(std::vector<std::shared_ptr<Type>> types);

  /* Try to settle `var` by the defaulting rules: every constraint on it must
   * be about it and nothing else, at least one must be the numeric class,
   * and a candidate must satisfy them all. Binds the variable and answers
   * true when one does. */
  bool defaultVariable(TypeManager &mgr, const std::string &var,
                       const std::vector<Pred> &preds) const;

  /* Every instance the environment holds, whatever class it belongs to. */
  std::vector<const InstanceInfo *> allInstances() const;

  /* -- Evidence ---------------------------------------------------------- */

  /* Build the evidence that answers `wanted`, given the dictionaries already
   * in hand. Nothing when neither those nor an instance answers it.
   *
   * This is `entail` again, saying not only that the constraint holds but
   * how: a superclass is taken out of the dictionary already held rather
   * than solved afresh, which is why the superclass chain is walked first. */
  std::shared_ptr<EvidenceTerm> solve(TypeManager &mgr,
                                      const std::vector<Given> &given,
                                      const Pred &wanted) const;

  /* The name of the head constructor of an instance, which is what its
   * generated dictionary function is named after. */
  static std::string headName(TypeManager &mgr, const Pred &head);
};

/* Whether two constraints say the same thing about the same type. */
bool samePred(TypeManager &mgr, const Pred &left, const Pred &right);

/* Whether a constraint is about a type variable and nothing more, which is
 * as far as reduction can take it. */
bool inHnf(TypeManager &mgr, const Pred &pred);

} // namespace sem
} // namespace ff
