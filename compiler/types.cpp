#include "types.hpp"
#include "parsed_type.hpp"
#include "error.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <sstream>
#include <utility>

namespace ff {
namespace sem {

std::string TypeManager::newTypeName() noexcept {
  int temp = this->lastId++;
  std::string str = "";

  while (temp != -1) {
    str += (char)('a' + (temp % 26));
    temp = temp / 26 - 1;
  }

  std::reverse(str.begin(), str.end());

  /* Quoted so a fresh variable can never collide with one the program wrote
   * itself: the lexer cannot produce an identifier containing a quote, and
   * both kinds are looked up by name in the same substitution map. */
  return "'" + str;
}

std::shared_ptr<Type> TypeManager::substitute(
    const std::map<std::string, std::shared_ptr<Type>> &subst,
    const std::shared_ptr<Type> &t) {
  std::shared_ptr<Type> temp = t;
  while (TypeVar *var = dynamic_cast<TypeVar *>(temp.get())) {
    auto substIt = subst.find(var->getName());
    if (substIt != subst.end())
      return substIt->second;
    auto varIt = types.find(var->getName());
    if (varIt == types.end())
      return t;
    temp = varIt->second;
  }

  if (TypeArr *arr = dynamic_cast<TypeArr *>(temp.get())) {
    auto leftResult = substitute(subst, arr->getLeft());
    auto rightResult = substitute(subst, arr->getRight());

    if (leftResult == arr->getLeft() && rightResult == arr->getRight())
      return t;

    return std::shared_ptr<Type>(new TypeArr(leftResult, rightResult));
  } else if (TypeApp *app = dynamic_cast<TypeApp *>(temp.get())) {
    auto constructorResult = substitute(subst, app->constructor);
    bool argChanged = false;
    std::vector<std::shared_ptr<Type>> newArgs;
    for (auto &arg : app->arguments) {
      auto argResult = substitute(subst, arg);
      argChanged |= argResult != arg;
      newArgs.push_back(std::move(argResult));
    }

    if (constructorResult == app->constructor && !argChanged)
      return t;

    TypeApp *newApp = new TypeApp(std::move(constructorResult));
    std::swap(newApp->arguments, newArgs);
    return std::shared_ptr<Type>(newApp);
  }
  return t;
}

std::shared_ptr<Type> TypeManager::newType() noexcept {
  return std::shared_ptr<Type>(new TypeVar(newTypeName()));
}

std::shared_ptr<Type> TypeManager::newArrowType() noexcept {
  return std::shared_ptr<Type>(new TypeArr(newType(), newType()));
}

std::shared_ptr<Type> TypeManager::resolve(std::shared_ptr<Type> t,
                                           TypeVar *&var) const {
  TypeVar *cast;

  var = nullptr;
  while ((cast = dynamic_cast<TypeVar *>(t.get()))) {
    auto it = types.find(cast->getName());

    if (it == types.end()) {
      var = cast;
      break;
    }
    t = it->second;
  }

  return t;
}

void TypeManager::unify(std::shared_ptr<Type> l, std::shared_ptr<Type> r,
                        const std::optional<yy::location> &loc) {
  TypeVar *lvar, *rvar;
  TypeArr *larr, *rarr;
  TypeBase *lid, *rid;
  TypeApp *lapp, *rapp;

  l = resolve(l, lvar);
  r = resolve(r, rvar);

  if (lvar) {
    bind(lvar->getName(), r);
    return;
  } else if (rvar) {
    bind(rvar->getName(), l);
    return;
  } else if ((larr = dynamic_cast<TypeArr *>(l.get())) &&
             (rarr = dynamic_cast<TypeArr *>(r.get()))) {
    unify(larr->getLeft(), rarr->getLeft(), loc);
    unify(larr->getRight(), rarr->getRight(), loc);
    return;
  } else if ((lid = dynamic_cast<TypeBase *>(l.get())) &&
             (rid = dynamic_cast<TypeBase *>(r.get()))) {
    if (lid->getName() == rid->getName() && lid->getArity() == rid->getArity())
      return;
  } else if ((lapp = dynamic_cast<TypeApp *>(l.get())) &&
             (rapp = dynamic_cast<TypeApp *>(r.get()))) {
    unify(lapp->constructor, rapp->constructor, loc);
    auto leftIt = lapp->arguments.begin();
    auto rightIt = rapp->arguments.begin();
    while (leftIt != lapp->arguments.end() &&
           rightIt != rapp->arguments.end()) {
      unify(*leftIt, *rightIt, loc);
      leftIt++, rightIt++;
    }
    return;
  }

  throw ff::UnificationError(l, r, loc);
}

void TypeManager::bind(const std::string &s, std::shared_ptr<Type> t) {
  TypeVar *other = dynamic_cast<TypeVar *>(t.get());

  if (other && other->getName() == s)
    return;
  types[s] = t;
}

std::string dictionaryConstructorName(const std::string &className) {
  return className + generatedMarker + "dict";
}

std::string methodSelectorName(const std::string &className,
                               const std::string &methodName) {
  return className + generatedMarker + "sel" + generatedMarker + methodName;
}

std::string superSelectorName(const std::string &className,
                              const std::string &superName) {
  return className + generatedMarker + "super" + generatedMarker + superName;
}

std::string instanceDictionaryName(const std::string &className,
                                   const std::string &headName) {
  return className + generatedMarker + headName + generatedMarker + "inst";
}

std::string defaultMethodName(const std::string &className,
                              const std::string &methodName) {
  return className + generatedMarker + "default" + generatedMarker + methodName;
}

std::string dictionaryParamName(const std::string &className,
                                std::size_t index) {
  return "d" + std::string(generatedMarker) + className +
         std::string(generatedMarker) + std::to_string(index);
}

std::shared_ptr<EvidenceTerm> EvidenceTerm::ofParameter(std::string name) {
  return std::shared_ptr<EvidenceTerm>(
      new EvidenceTerm(std::move(name), true, {}));
}

std::shared_ptr<EvidenceTerm> EvidenceTerm::ofApplication(
    std::string symbol, std::vector<std::shared_ptr<EvidenceTerm>> arguments) {
  return std::shared_ptr<EvidenceTerm>(
      new EvidenceTerm(std::move(symbol), false, std::move(arguments)));
}

void EvidenceTerm::collectParameters(std::set<std::string> &into) const {
  if (parameter)
    into.insert(name);

  for (auto &argument : arguments)
    argument->collectParameters(into);
}

void EvidenceTerm::print(std::ostream &to) const {
  if (arguments.empty()) {
    to << name;
    return;
  }

  to << "(" << name;
  for (auto &argument : arguments) {
    to << " ";
    argument->print(to);
  }
  to << ")";
}

void TypeManager::want(Pred pred, const yy::location &loc,
                       std::shared_ptr<EvidenceSlot> slot) {
  this->wanted.push_back(Wanted(std::move(pred), loc, std::move(slot)));
}

void TypeManager::want(Wanted wanted) {
  this->wanted.push_back(std::move(wanted));
}

std::size_t TypeManager::wantedMark() const noexcept {
  return this->wanted.size();
}

std::vector<Wanted> TypeManager::takeWantedFrom(std::size_t mark) {
  assert(mark <= this->wanted.size());

  std::vector<Wanted> taken(
      std::make_move_iterator(this->wanted.begin() + mark),
      std::make_move_iterator(this->wanted.end()));
  this->wanted.erase(this->wanted.begin() + mark, this->wanted.end());

  return taken;
}

std::shared_ptr<Type> TypeScheme::instantiate(
    TypeManager &mgr, const yy::location &loc,
    std::vector<std::shared_ptr<EvidenceSlot>> *slots) const {
  /* One slot per constraint, handed to the manager with the constraint and
   * kept by the use, so that solving the constraint tells the use what to
   * apply itself to. */
  auto take = [&](Pred pred) {
    std::shared_ptr<EvidenceSlot> slot;
    if (slots) {
      slot = std::shared_ptr<EvidenceSlot>(new EvidenceSlot());
      slots->push_back(slot);
    }
    mgr.want(std::move(pred), loc, std::move(slot));
  };

  if (forall.size() == 0) {
    /* A scheme quantifying nothing still stands for what it holds under: an
     * instance method is checked with its own instance context in hand. */
    for (auto &pred : context)
      take(pred);
    return monotype;
  }

  std::map<std::string, std::shared_ptr<Type>> subst;
  for (auto &var : forall) {
    subst[var] = mgr.newType();
  }

  for (auto &pred : context)
    take(Pred(pred.className, mgr.substitute(subst, pred.type)));

  return mgr.substitute(subst, monotype);
}

void TypeScheme::print(const TypeManager &mgr, std::ostream &to) const {
  if (forall.size() != 0) {
    to << "forall ";
    for (auto &var : forall) {
      to << var << " ";
    }
    to << ". ";
  }
  for (auto &pred : context) {
    pred.print(mgr, to);
    to << " => ";
  }
  monotype->print(mgr, to);
}

const std::string &TypeNamer::nameOf(const std::string &var) {
  auto it = names.find(var);
  if (it != names.end())
    return it->second;

  /* a, b, ... z, then a1, b1, and so on: enough names never to repeat, and
   * the plain letters for the many types that need only a few. */
  std::size_t index = names.size();
  std::string name(1, (char)('a' + (index % 26)));
  if (index >= 26)
    name += std::to_string(index / 26);

  return names.emplace(var, std::move(name)).first->second;
}

namespace {

/* Whether writing `type` where an argument is expected needs parentheses
 * round it. A name and a variable stand alone; everything else is built out
 * of more than one piece.
 *
 * What a type is has to be read through the substitution: a variable
 * unification has since settled to a function is a function here. */
bool needsParens(const TypeManager &mgr, const std::shared_ptr<Type> &type) {
  TypeVar *var;
  auto resolved = mgr.resolve(type, var);
  if (var)
    return false;

  if (auto *app = dynamic_cast<TypeApp *>(resolved.get()))
    return !app->arguments.empty();
  return dynamic_cast<TypeArr *>(resolved.get()) != nullptr;
}

bool isArrow(const TypeManager &mgr, const std::shared_ptr<Type> &type) {
  TypeVar *var;
  auto resolved = mgr.resolve(type, var);
  return !var && dynamic_cast<TypeArr *>(resolved.get()) != nullptr;
}

void printReadableArgument(const TypeManager &mgr,
                           const std::shared_ptr<Type> &type, TypeNamer &namer,
                           std::ostream &to) {
  bool parens = needsParens(mgr, type);
  if (parens)
    to << "(";
  printReadable(mgr, type, namer, to);
  if (parens)
    to << ")";
}

} // namespace

void printReadable(const TypeManager &mgr, const std::shared_ptr<Type> &type,
                   TypeNamer &namer, std::ostream &to) {
  TypeVar *var;
  auto resolved = mgr.resolve(type, var);

  if (var) {
    to << namer.nameOf(var->getName());
    return;
  }

  if (auto *arr = dynamic_cast<TypeArr *>(resolved.get())) {
    /* Arrows group to the right, so only a function on the left of one needs
     * to be written out with parentheses. */
    bool parens = isArrow(mgr, arr->getLeft());
    if (parens)
      to << "(";
    printReadable(mgr, arr->getLeft(), namer, to);
    if (parens)
      to << ")";

    to << " -> ";
    printReadable(mgr, arr->getRight(), namer, to);
    return;
  }

  if (auto *app = dynamic_cast<TypeApp *>(resolved.get())) {
    printReadable(mgr, app->constructor, namer, to);
    for (auto &argument : app->arguments) {
      to << " ";
      printReadableArgument(mgr, argument, namer, to);
    }
    return;
  }

  if (auto *base = dynamic_cast<TypeBase *>(resolved.get())) {
    to << base->getName();
    return;
  }

  resolved->print(mgr, to);
}

std::string structuralKey(const TypeManager &mgr,
                          const std::shared_ptr<Type> &type) {
  TypeVar *var;
  auto resolved = mgr.resolve(type, var);

  if (var)
    return var->getName();

  if (auto *arr = dynamic_cast<TypeArr *>(resolved.get()))
    return "(" + structuralKey(mgr, arr->getLeft()) + "->" +
           structuralKey(mgr, arr->getRight()) + ")";

  if (auto *app = dynamic_cast<TypeApp *>(resolved.get())) {
    std::string key = "(" + structuralKey(mgr, app->constructor);
    for (auto &argument : app->arguments)
      key += " " + structuralKey(mgr, argument);
    return key + ")";
  }

  if (auto *base = dynamic_cast<TypeBase *>(resolved.get()))
    return base->getName();

  return "?";
}

void printReadable(const TypeManager &mgr, const Pred &pred, TypeNamer &namer,
                   std::ostream &to) {
  to << pred.className << " ";
  printReadableArgument(mgr, pred.type, namer, to);
}

void printReadable(const TypeManager &mgr, const TypeScheme &scheme,
                   std::ostream &to) {
  TypeNamer namer;

  /* The constraints are named before the type is, so that the variable a
   * context talks about is the first one the reader meets. */
  if (!scheme.context.empty()) {
    bool several = scheme.context.size() > 1;
    if (several)
      to << "(";
    for (auto it = scheme.context.begin(); it != scheme.context.end(); it++) {
      if (it != scheme.context.begin())
        to << ", ";
      printReadable(mgr, *it, namer, to);
    }
    if (several)
      to << ")";
    to << " => ";
  }

  printReadable(mgr, scheme.monotype, namer, to);
}

void Pred::print(const TypeManager &mgr, std::ostream &to) const {
  to << this->className << " ";
  this->type->print(mgr, to);
}

void TypeVar::print(const TypeManager &mgr, std::ostream &to) const {
  auto it = mgr.types.find(this->name);
  if (it != mgr.types.end()) {
    it->second->print(mgr, to);
  } else {
    to << this->name;
  }
}

void TypeBase::print(const TypeManager &, std::ostream &to) const {
  to << this->name;
}

void TypeArr::print(const TypeManager &mgr, std::ostream &to) const {
  left->print(mgr, to);
  to << " -> (";
  right->print(mgr, to);
  to << ")";
}

void TypeApp::print(const TypeManager &mgr, std::ostream &to) const {
  constructor->print(mgr, to);
  to << "* ";
  for (auto &arg : arguments) {
    to << " ";
    arg->print(mgr, to);
  }
}

void TypeManager::findFree(const std::shared_ptr<Type> &t,
                           std::set<std::string> &into) const {
  TypeVar *var;
  auto resolved = resolve(t, var);

  if (var) {
    into.insert(var->getName());
  } else if (TypeArr *arr = dynamic_cast<TypeArr *>(resolved.get())) {
    findFree(arr->getLeft(), into);
    findFree(arr->getRight(), into);
  } else if (TypeApp *app = dynamic_cast<TypeApp *>(resolved.get())) {
    findFree(app->constructor, into);
    for (auto &arg : app->arguments)
      findFree(arg, into);
  }
}

std::shared_ptr<Type> ParsedTypeApp::toType(const std::set<std::string> &vars,
                                            const TypeContext &typeCtx,
                                            const yy::location &loc) const {
  auto parentType = typeCtx.lookupType(name);
  if (parentType == nullptr)
    throw ff::TypeError("unknown type " + name, loc);

  /* Only data types and the built-in bases are ever bound as type names. */
  TypeBase *baseType = dynamic_cast<TypeBase *>(parentType.get());
  assert(baseType != nullptr);

  if (std::cmp_not_equal(baseType->getArity(), arguments.size())) {
    std::ostringstream errorStream;
    errorStream << "invalid application of type " << name << " ("
                << baseType->getArity() << " argument(s) expected, but "
                << arguments.size() << " provided)";

    throw ff::TypeError(errorStream.str(), loc);
  }

  TypeApp *newApp = new TypeApp(std::move(parentType));
  std::shared_ptr<Type> toReturn(newApp);
  for (auto &arg : arguments) {
    newApp->arguments.push_back(arg->toType(vars, typeCtx, loc));
  }

  return toReturn;
}

void ParsedTypeApp::collectVariables(std::set<std::string> &into) const {
  for (auto &arg : arguments)
    arg->collectVariables(into);
}

std::shared_ptr<Type> ParsedTypeVar::toType(const std::set<std::string> &vars,
                                            const TypeContext &,
                                            const yy::location &loc) const {
  if (vars.find(var) == vars.end())
    throw ff::TypeError("unbound type variable " + var, loc);

  return std::shared_ptr<Type>(new TypeVar(var));
}

void ParsedTypeVar::collectVariables(std::set<std::string> &into) const {
  into.insert(var);
}

std::shared_ptr<Type> ParsedTypeArr::toType(const std::set<std::string> &vars,
                                            const TypeContext &typeCtx,
                                            const yy::location &loc) const {
  auto newLeft = left->toType(vars, typeCtx, loc);
  auto newRight = right->toType(vars, typeCtx, loc);

  return std::shared_ptr<Type>(
      new TypeArr(std::move(newLeft), std::move(newRight)));
}

void ParsedTypeArr::collectVariables(std::set<std::string> &into) const {
  left->collectVariables(into);
  right->collectVariables(into);
}

/* The printers below write a type back out as source. Everything compound is
 * parenthesized: the parentheses do not survive parsing, so printing what was
 * printed once yields the same text again. */

void ParsedTypeApp::printSource(std::ostream &to) const {
  if (arguments.empty()) {
    to << name;
    return;
  }

  to << "(" << name;
  for (auto &argument : arguments) {
    to << " ";
    argument->printSource(to);
  }
  to << ")";
}

void ParsedTypeVar::printSource(std::ostream &to) const { to << var; }

void ParsedTypeArr::printSource(std::ostream &to) const {
  to << "(";
  left->printSource(to);
  to << " -> ";
  right->printSource(to);
  to << ")";
}

void ParsedPred::collectVariables(std::set<std::string> &into) const {
  for (auto &argument : arguments)
    argument->collectVariables(into);
}

void ParsedPred::printSource(std::ostream &to) const {
  to << className;
  for (auto &argument : arguments) {
    to << " ";
    argument->printSource(to);
  }
}

/* A context is always printed in its parenthesized form, so that one
 * predicate and several are written the same way. Both spellings parse to
 * the same list, so a program that wrote the bare form still round trips. */
void printContextSource(const ParsedContext &context, std::ostream &to) {
  if (context.empty())
    return;

  to << "(";
  for (auto it = context.begin(); it != context.end(); it++) {
    if (it != context.begin())
      to << ", ";
    (*it)->printSource(to);
  }
  to << ") => ";
}

} // namespace sem
} // namespace ff
