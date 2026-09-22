#include "compiler.hpp"
#include "ast.hpp"
#include "binop.hpp"
#include "context.hpp"
#include "error.hpp"
#include "parse_driver.hpp"
#include "types.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

/* Translation units making up the runtime, compiled fresh on every link.
 * Keep in sync with the contents of runtime/ -- runtime.h documents how the
 * pieces fit together. */
static const char *const runtimeSources[] = {
    "eval.c",  "gc.c",    "gmachine.c", "heap.c", "main.c",
    "panic.c", "stack.c", "stats.c",    "vec.c",
};

/* Definitions every program gets for free, parsed ahead of the source file
 * so that anything it declares can be redefined. */
static const char *const preludePath = "prelude/Base.ql";

/* The operators that answer with a Bool rather than an Int. They are set up
 * apart from the arithmetic ones because Bool comes from the prelude. */
static constexpr binop comparisonOps[] = {EQUALS,  NOTEQUALS,  LESS,
                                          GREATER, LESSEQUALS, GREATEREQUALS};

namespace ff {
namespace drv {

void Compiler::addDefaultTypes() {
  globalContext->bindType("Int",
                          std::unique_ptr<sem::Type>(new sem::TypeBase("Int")));
  addListType();
}

/* List is built in rather than declared in the prelude, so that a list
 * literal always has a type to build. Everything else about it matches
 * the data type it used to be: the same two constructors, bound as
 * globals under the same names and tags. */
void Compiler::addListType() {
  constexpr const char *itemVar = "a";

  sem::TypeData *listData = new sem::TypeData(sem::listTypeName, 1);
  std::shared_ptr<sem::Type> listType(listData);
  listData->constructors[sem::listNilName] = {sem::listNilTag};
  listData->constructors[sem::listConsName] = {sem::listConsTag};
  globalContext->bindType(sem::listTypeName, listType);

  std::shared_ptr<sem::Type> itemType(new sem::TypeVar(itemVar));
  sem::TypeApp *listApp = new sem::TypeApp(std::move(listType));
  std::shared_ptr<sem::Type> listOfItem(listApp);
  listApp->arguments.push_back(itemType);

  // Nil : forall a. List a
  std::shared_ptr<sem::TypeScheme> nilScheme(new sem::TypeScheme(listOfItem));
  nilScheme->forall.push_back(itemVar);
  globalContext->bind(sem::listNilName, std::move(nilScheme),
                      sem::Visibility::Global);

  // Cons : forall a. a -> List a -> List a
  std::shared_ptr<sem::Type> consType(new sem::TypeArr(
      std::move(itemType),
      std::shared_ptr<sem::Type>(new sem::TypeArr(listOfItem, listOfItem))));
  std::shared_ptr<sem::TypeScheme> consScheme(
      new sem::TypeScheme(std::move(consType)));
  consScheme->forall.push_back(itemVar);
  globalContext->bind(sem::listConsName, std::move(consScheme),
                      sem::Visibility::Global);
}

/* The compiler's own implementation of an operator, bound under a name the
 * prelude's instance for Int can call. The operator itself is surface syntax
 * for a method, and means whatever that method means. */
void Compiler::addBinopType(binop op, std::shared_ptr<sem::Type> type) {
  auto name = mangler.newMangledName(opAction(op));

  globalContext->bind(opPrimitive(op), std::move(type),
                      sem::Visibility::Global);
  globalContext->setMangledName(opPrimitive(op), name);
}

void Compiler::addDefaultFunctionTypes() {

  std::shared_ptr<sem::Type> intType = globalContext->lookupType("Int");
  assert(intType != nullptr);
  std::shared_ptr<sem::Type> intTypeApp =
      std::shared_ptr<sem::Type>(new sem::TypeApp(intType));

  std::shared_ptr<sem::Type> closedIntOpType(new sem::TypeArr(
      intTypeApp,
      std::shared_ptr<sem::Type>(new sem::TypeArr(intTypeApp, intTypeApp))));

  constexpr binop closedOps[] = {PLUS, MINUS, TIMES, DIVIDE};
  for (auto &op : closedOps)
    addBinopType(op, closedIntOpType);

  addComposeType();
}

/* Composition works on any two functions that fit together, so its type is
 * forall a b c. (b -> c) -> (a -> b) -> a -> c. Nothing about it depends on
 * the prelude, unlike the comparisons. */
void Compiler::addComposeType() {
  constexpr const char *argumentVar = "a";
  constexpr const char *middleVar = "b";
  constexpr const char *resultVar = "c";

  std::shared_ptr<sem::Type> argument(new sem::TypeVar(argumentVar));
  std::shared_ptr<sem::Type> middle(new sem::TypeVar(middleVar));
  std::shared_ptr<sem::Type> result(new sem::TypeVar(resultVar));

  std::shared_ptr<sem::Type> outer(new sem::TypeArr(middle, result));
  std::shared_ptr<sem::Type> inner(new sem::TypeArr(argument, middle));
  std::shared_ptr<sem::Type> composed(new sem::TypeArr(argument, result));

  std::shared_ptr<sem::Type> composeType(new sem::TypeArr(
      std::move(outer), std::shared_ptr<sem::Type>(new sem::TypeArr(
                            std::move(inner), std::move(composed)))));

  std::shared_ptr<sem::TypeScheme> scheme(
      new sem::TypeScheme(std::move(composeType)));
  scheme->forall = {argumentVar, middleVar, resultVar};

  globalContext->bind(sem::composeName, std::move(scheme),
                      sem::Visibility::Global);
  globalContext->setMangledName(sem::composeName,
                                mangler.newMangledName(sem::composeAction));
}

/* The comparisons hand back a Bool, which the prelude declares like any
 * other data type, so their types cannot be built until it has been read. */
void Compiler::addComparisonFunctionTypes() {
  std::shared_ptr<sem::Type> intType = globalContext->lookupType("Int");
  assert(intType != nullptr);

  std::shared_ptr<sem::Type> boolType =
      globalContext->lookupType(sem::boolTypeName);
  auto *boolData = dynamic_cast<sem::TypeData *>(boolType.get());
  if (!boolData)
    throw TypeError(std::string("a comparison needs the ") + sem::boolTypeName +
                    " data type, which is not defined");

  auto trueIt = boolData->constructors.find(sem::boolTrueName);
  auto falseIt = boolData->constructors.find(sem::boolFalseName);
  if (trueIt == boolData->constructors.end() ||
      falseIt == boolData->constructors.end())
    throw TypeError(std::string("a comparison needs the ") + sem::boolTypeName +
                    " type to have the " + sem::boolTrueName + " and " +
                    sem::boolFalseName + " constructors");

  boolTrueTag = trueIt->second.tag;
  boolFalseTag = falseIt->second.tag;

  std::shared_ptr<sem::Type> intTypeApp(new sem::TypeApp(std::move(intType)));
  std::shared_ptr<sem::Type> boolTypeApp(new sem::TypeApp(std::move(boolType)));

  std::shared_ptr<sem::Type> comparisonType(new sem::TypeArr(
      intTypeApp,
      std::shared_ptr<sem::Type>(new sem::TypeArr(intTypeApp, boolTypeApp))));

  for (auto &op : comparisonOps)
    addBinopType(op, comparisonType);
}

void Compiler::parseFile(const std::string &path) {
  ParseDriver driver(fileManager, globalDefs, path);
  if (!driver())
    throw CompilerError("could not open file " + path);
}

void Compiler::parse() {
  parseFile(preludePath);
  parseFile(inputFile);
}

/* What every class and instance in the program comes to, checked over before
 * anything asks a question of it. The data declarations are bound first: an
 * instance head names a type, so the types have to be in scope, but nothing
 * about a class depends on inference having started. */
void Compiler::buildClassEnv() {
  globalDefs.insertDataTypes(globalContext);
  classEnv.build(globalDefs, *globalContext);
}

/* Write the program out instead of compiling it. The prelude is left unread:
 * a dump is asked for in order to see one file, and putting the prelude in
 * front of it would bury the answer and stop a printed program from being
 * handed straight back in. */
void Compiler::dump() {
  parseFile(inputFile);

  switch (dumpKind) {
  case DumpKind::Ast:
    globalDefs.print(0, std::cout);
    break;
  case DumpKind::Source:
    globalDefs.printSource(std::cout);
    break;
  case DumpKind::Classes:
    buildClassEnv();
    classEnv.print(std::cout);
    break;
  case DumpKind::Types:
  case DumpKind::Core:
  case DumpKind::None:
    break;
  }
}

void Compiler::typecheck() {
  buildClassEnv();

  manager.setClassEnv(classEnv);

  /* The one type an ambiguous numeric variable may be settled to. */
  std::vector<std::shared_ptr<sem::Type>> defaults;
  defaults.push_back(std::shared_ptr<sem::Type>(
      new sem::TypeApp(globalContext->lookupType("Int"))));
  classEnv.setDefaults(std::move(defaults));

  classEnv.bindMethods(*globalContext);

  std::set<std::string> freeVariables;
  globalDefs.findFree(manager, globalContext, ff::sem::Visibility::Global,
                      freeVariables);

  /* Bool is only in scope once the prelude has been walked, which findFree
   * has just done. */
  addComparisonFunctionTypes();

  /* Nothing encloses the top level, so anything still free here that is not
   * already bound -- a constructor, an operator -- has no definition. */
  for (auto &free : freeVariables) {
    if (globalContext->lookup(free) != nullptr)
      continue;

    /* A name nothing can write is one the compiler asked for on a program's
     * behalf: an operator, or a written number, needing the class that says
     * what it means. */
    if (free.find(sem::generatedMarker) != std::string::npos)
      throw ff::TypeError("an operator or a written number needs the class "
                          "declaring " +
                          free.substr(free.rfind(sem::generatedMarker) + 1) +
                          ", which the prelude did not provide");

    throw ff::TypeError("undefined variable " + free);
  }

  globalDefs.typecheck(manager);
  typecheckDefaults();
  typecheckInstances();
  resolveRemaining();
  verifyEvidence();
}

/* Check the default a class supplies for a method it does not insist every
 * instance implement.
 *
 * A default is compiled once, as a function of the dictionary it will be a
 * field of, so that it may call any other method of the same class. That
 * dictionary is the only thing it holds under. */
void Compiler::typecheckDefaults() {
  for (auto &pair : globalDefs.defsClass) {
    auto &declaration = *pair.second;
    const sem::ClassInfo *info = classEnv.lookup(declaration.getName());
    /* Every class the program declared is in the environment. */
    assert(info != nullptr);

    for (auto &method : declaration.methods) {
      if (!method->body)
        continue;

      const sem::MethodInfo *methodInfo = info->lookupMethod(method->name);
      assert(methodInfo != nullptr);

      /* The class variable stands for whatever the instance turns out to be
       * about, so the default is checked at a variable of its own. */
      std::map<std::string, std::shared_ptr<sem::Type>> subst;
      for (auto &var : methodInfo->forall)
        subst[var] = manager.newType();

      auto expected = manager.substitute(subst, methodInfo->type);
      sem::Pred held(info->name, subst.at(info->var));

      auto paramName = sem::dictionaryParamName(info->name, 0);
      std::vector<sem::Given> evidence;
      evidence.push_back(
          sem::Given(held, sem::EvidenceTerm::ofParameter(paramName)));

      std::size_t mark = manager.wantedMark();

      method->returnDescription = "the default for " + method->name +
                                  " does not have the type " + info->name +
                                  " declares for it";
      method->findFree(manager, globalContext);
      for (auto &free : method->freeVariables) {
        if (globalContext->lookup(free) == nullptr)
          throw ff::TypeError("undefined variable " + free, method->loc);
      }

      try {
        manager.unify(method->fullType, expected, method->loc);
      } catch (const ff::UnificationError &) {
        throw ff::UnificationError(expected, method->fullType, method->loc,
                                   method->returnDescription);
      }
      method->typecheck(manager);

      auto methodWanted = manager.takeWantedFrom(mark);
      std::vector<sem::Pred> given{held};

      for (auto &one : classEnv.reduce(manager, methodWanted, given)) {
        if (classEnv.entail(manager, given, one.pred))
          continue;

        sem::TypeNamer namer;
        std::ostringstream errorStream;
        errorStream << "the default for " << method->name << " needs ";
        sem::printReadable(manager, one.pred, namer, errorStream);
        errorStream << ", which " << info->name << " does not hold under";

        throw ff::TypeError(errorStream.str(), one.loc);
      }

      for (auto &one : methodWanted) {
        auto term = classEnv.solve(manager, evidence, one.pred);
        /* The reduction above already established that each is answerable. */
        assert(term != nullptr);

        if (one.slot)
          one.slot->term = std::move(term);
      }

      /* The dictionary is an ordinary first parameter, ahead of the ones the
       * method wrote, and is bound where anything nested will find it. */
      method->dictionaryParams.push_back(paramName);
      method->varContext->bind(paramName, manager.newType(),
                               sem::Visibility::Local);

      std::vector<std::unique_ptr<Param>> withDictionary;
      withDictionary.push_back(
          std::unique_ptr<Param>(new Param(paramName, nullptr, method->loc)));
      withDictionary.insert(withDictionary.end(),
                            std::make_move_iterator(method->params.begin()),
                            std::make_move_iterator(method->params.end()));
      method->params = std::move(withDictionary);
    }
  }
}

/* Check what each instance provides against what its class asked for.
 *
 * This runs after every top level group, and can: an instance method is
 * reached through a dictionary rather than by name, so nothing at the top
 * level has a type that depends on one, while the method itself is free to
 * call anything the program defines. */
void Compiler::typecheckInstances() {
  for (const sem::InstanceInfo *instance : classEnv.allInstances()) {
    const sem::ClassInfo *info = classEnv.lookup(instance->qual.head.className);
    /* An instance is only recorded under a class that exists. */
    assert(info != nullptr);

    /* The instance's variables stand for whatever a use of it turns out to
     * be about, so each instance is checked with its own. */
    std::map<std::string, std::shared_ptr<sem::Type>> subst;
    for (auto &var : instance->forall)
      subst[var] = manager.newType();

    auto headType = manager.substitute(subst, instance->qual.head.type);

    /* What the instance was allowed to assume, in the canonical order, one
     * dictionary parameter each. The dictionary this instance builds is a
     * function of them, and its method bodies reach for them. */
    std::vector<sem::Pred> given;
    for (auto &pred : instance->qual.preds)
      given.push_back(
          sem::Pred(pred.className, manager.substitute(subst, pred.type)));

    std::sort(given.begin(), given.end(),
              [this](const sem::Pred &left, const sem::Pred &right) {
                if (left.className != right.className)
                  return left.className < right.className;
                return sem::structuralKey(manager, left.type) <
                       sem::structuralKey(manager, right.type);
              });

    std::vector<sem::Given> evidence;
    instance->declaration->dictionaryParams.clear();

    /* The dictionaries are bound where the methods will look for them, so
     * that lifting a method captures them the way it captures any other
     * local. They belong to the instance, not to any one method. */
    auto instanceContext = sem::typeScope(globalContext);

    for (std::size_t i = 0; i < given.size(); i++) {
      auto paramName = sem::dictionaryParamName(given[i].className, i);
      instance->declaration->dictionaryParams.push_back(paramName);
      evidence.push_back(
          sem::Given(given[i], sem::EvidenceTerm::ofParameter(paramName)));
      instanceContext->bind(paramName, manager.newType(),
                            sem::Visibility::Local);
    }

    for (auto &method : instance->declaration->methods) {
      const sem::MethodInfo *methodInfo = info->lookupMethod(method->name);
      /* Every method of an instance was checked to belong to its class. */
      assert(methodInfo != nullptr);

      /* The class signature with the instance head standing where the class
       * variable stood, and whatever else the method quantifies over given
       * variables of its own. */
      auto methodSubst = subst;
      methodSubst[info->var] = headType;
      for (auto &var : methodInfo->forall) {
        if (methodSubst.find(var) == methodSubst.end())
          methodSubst[var] = manager.newType();
      }
      auto expected = manager.substitute(methodSubst, methodInfo->type);

      std::size_t mark = manager.wantedMark();

      method->returnDescription = "the method " + method->name +
                                  " does not have the type " + info->name +
                                  " declares for it";

      method->findFree(manager, instanceContext);
      for (auto &free : method->freeVariables) {
        if (instanceContext->lookup(free) == nullptr)
          throw ff::TypeError("undefined variable " + free, method->loc);
      }

      /* Pinned down before the body is looked at, so that a body that does
       * not fit is reported against the part of it that does not. The two
       * whole types are worth showing here, since what the method has to be
       * is not written anywhere near it. */
      try {
        manager.unify(method->fullType, expected, method->loc);
      } catch (const ff::UnificationError &) {
        throw ff::UnificationError(expected, method->fullType, method->loc,
                                   method->returnDescription);
      }
      method->typecheck(manager);

      auto methodWanted = manager.takeWantedFrom(mark);

      for (auto &one : classEnv.reduce(manager, methodWanted, given)) {
        if (classEnv.entail(manager, given, one.pred))
          continue;

        sem::TypeNamer namer;
        std::ostringstream errorStream;
        errorStream << "the method " << method->name << " needs ";
        sem::printReadable(manager, one.pred, namer, errorStream);
        errorStream << ", which the instance does not hold under";

        throw ff::TypeError(errorStream.str(), one.loc);
      }

      /* Everything the body wanted is answered by what the instance holds
       * under, so each use can be told what to apply itself to. */
      for (auto &one : methodWanted) {
        auto term = classEnv.solve(manager, evidence, one.pred);
        /* The reduction above already established that each is answerable. */
        assert(term != nullptr);

        if (one.slot)
          one.slot->term = std::move(term);
      }
    }

    layOutDictionary(*instance, *info, headType, evidence);
  }
}

/* What goes in each field of the dictionary this instance builds: the
 * superclass dictionaries first, then one entry per method of the class, in
 * the order the class declared them. */
void Compiler::layOutDictionary(const sem::InstanceInfo &instance,
                                const sem::ClassInfo &info,
                                const std::shared_ptr<sem::Type> &headType,
                                const std::vector<sem::Given> &evidence) {
  auto &declaration = *instance.declaration;

  declaration.mangledName = sem::instanceDictionaryName(
      info.name, sem::ClassEnv::headName(manager, instance.qual.head));

  for (auto &super : info.supers) {
    sem::Pred needed(super.className, headType);
    auto term = classEnv.solve(manager, evidence, needed);

    /* A class says an instance of it is already an instance of each of its
     * superclasses. Nothing has checked that until now, because the answer
     * is the dictionary field itself. */
    if (!term) {
      sem::TypeNamer namer;
      std::ostringstream errorStream;
      errorStream << "the instance ";
      sem::printReadable(manager, instance.qual.head, namer, errorStream);
      errorStream << " needs ";
      sem::printReadable(manager, needed, namer, errorStream);
      errorStream << ", which no instance provides";

      throw ff::TypeError(errorStream.str(), declaration.loc);
    }

    DictionaryField field;
    field.evidence = std::move(term);
    declaration.dictionaryFields.push_back(std::move(field));
  }

  for (auto &method : info.methods) {
    DictionaryField field;

    for (auto &provided : declaration.methods) {
      if (provided->name != method.name)
        continue;

      field.method = provided.get();
      break;
    }

    /* What the instance did not provide, the class did: every method was
     * checked to be one or the other. */
    if (!field.method)
      field.defaultSymbol = sem::defaultMethodName(info.name, method.name);

    declaration.dictionaryFields.push_back(std::move(field));
  }
}

/* Constraints that reached the top level unanswered. Nothing encloses it, so
 * each is settled by defaulting or is a mistake. */
void Compiler::resolveRemaining() {
  auto original = manager.takeWantedFrom(0);
  auto leftover = classEnv.reduce(manager, original);

  if (!leftover.empty()) {
    std::vector<sem::Pred> preds;
    std::set<std::string> vars;
    for (auto &one : leftover) {
      preds.push_back(one.pred);
      manager.findFree(one.pred.type, vars);
    }

    for (auto &var : vars)
      classEnv.defaultVariable(manager, var, preds);

    leftover = classEnv.reduce(manager, std::move(leftover));
  }

  if (!leftover.empty()) {
    sem::TypeNamer namer;
    std::ostringstream errorStream;
    errorStream << "nothing settles ";
    sem::printReadable(manager, leftover.front().pred, namer, errorStream);

    throw ff::TypeError(errorStream.str(), leftover.front().loc);
  }

  /* Nothing encloses the top level, so what is left is answered by the
   * instances alone. */
  for (auto &one : original) {
    auto term = classEnv.solve(manager, {}, one.pred);
    /* The reduction above already established that each is answerable. */
    assert(term != nullptr);

    if (one.slot)
      one.slot->term = std::move(term);
  }
}

/* After elaboration nothing is left unanswered, and every dictionary a body
 * reaches for is one it was handed. The first is asserted where the evidence
 * is walked; this is the second. */
void Compiler::verifyEvidence() {
  std::set<std::string> escaping;
  globalDefs.findEvidence(escaping);

  if (!escaping.empty())
    throw ff::CompilerError("the dictionary " + *escaping.begin() +
                            " is used where nothing provides it");

  for (auto &pair : globalDefs.defsClass) {
    for (auto &method : pair.second->methods) {
      if (!method->body)
        continue;

      std::set<std::string> used;
      method->findEvidence(used);

      if (!used.empty())
        throw ff::CompilerError("the dictionary " + *used.begin() +
                                    " is used where nothing provides it",
                                method->loc);
    }
  }

  for (const sem::InstanceInfo *instance : classEnv.allInstances()) {
    for (auto &method : instance->declaration->methods) {
      std::set<std::string> used;
      method->findEvidence(used);

      for (auto &param : instance->declaration->dictionaryParams)
        used.erase(param);

      if (!used.empty())
        throw ff::CompilerError("the dictionary " + *used.begin() +
                                    " is used where nothing provides it",
                                method->loc);
    }
  }
}

void Compiler::printTypes() const {
  for (auto &pair : globalContext->getNames()) {
    std::cout << pair.first << " : ";
    sem::printReadable(manager, *pair.second->scheme, std::cout);
    std::cout << std::endl;
  }
}

/*
 * Two rewrites, both on the evidence a use was told to hand over, and both
 * turning on the same thing: whether the dictionary being selected from is
 * one whose construction is written right there.
 *
 * Known dictionary:
 * Selecting from a construction is the field itself. A
 * superclass becomes the evidence that instance was
 * built with, and a method becomes a direct reference
 * to the function implementing it, so the dictionary
 * need not be built at all.
 *
 * Constant sharing:
 * A dictionary that reaches for nothing is the same
 * dictionary everywhere, so it is given a name of its
 * own and built once for the program rather than
 * wherever it is mentioned, which for a mention inside
 * a loop is once per iteration.
 *
 * Both run after lifting, since what a method is called and what it captured
 * are only settled then. Nothing here can introduce a dictionary a definition
 * was not already reaching for: every term either shrinks or is replaced by
 * one of its own subterms.
 */

namespace {

bool isClosed(const sem::EvidenceTerm &term) {
  if (term.parameter)
    return false;

  for (auto &argument : term.arguments) {
    if (!isClosed(*argument))
      return false;
  }
  return true;
}

std::string termKey(const sem::EvidenceTerm &term) {
  std::ostringstream stream;
  term.print(stream);
  return stream.str();
}

/* A term with the dictionaries an instance holds under put in place of the
 * parameters standing for them. */
std::shared_ptr<sem::EvidenceTerm>
substituteTerm(const sem::EvidenceTerm &term,
               const std::vector<std::string> &params,
               const std::vector<std::shared_ptr<sem::EvidenceTerm>> &args) {
  if (term.parameter) {
    for (std::size_t i = 0; i < params.size(); i++) {
      if (params[i] == term.name) {
        /* An instance is only ever applied to as many dictionaries as it
         * holds under. */
        assert(i < args.size());
        return args[i];
      }
    }
    return sem::EvidenceTerm::ofParameter(term.name);
  }

  std::vector<std::shared_ptr<sem::EvidenceTerm>> rewritten;
  for (auto &argument : term.arguments)
    rewritten.push_back(substituteTerm(*argument, params, args));

  return sem::EvidenceTerm::ofApplication(term.name, std::move(rewritten));
}

} // namespace

void Compiler::forEachNode(const std::function<void(Ast &)> &visit) {
  for (auto &pair : globalDefs.defsDefn)
    pair.second->forEachNode(visit);
  for (auto *definition : globalScope.getDefinitions())
    definition->forEachNode(visit);
  for (auto &pair : globalDefs.defsClass) {
    for (auto &method : pair.second->methods) {
      if (method->body)
        method->forEachNode(visit);
    }
  }
}

void Compiler::forEachReference(const std::function<void(AstLid &)> &visit) {
  forEachNode([&](Ast &node) {
    if (auto *reference = dynamic_cast<AstLid *>(&node))
      visit(*reference);
  });
}

void Compiler::forEachEvidenceSlot(
    const std::function<void(std::shared_ptr<sem::EvidenceSlot> &)> &visit) {
  forEachReference([&](AstLid &reference) {
    for (auto &slot : reference.evidence)
      visit(slot);
  });
}

void Compiler::optimizeEvidence() {
  for (auto &instance : globalDefs.defsInstance)
    instancesBySymbol[instance->mangledName] = instance.get();

  /* A written number at the machine type is the machine number: the
   * instance for it hands back what it was given, so the call that would
   * convert it is not made at all. Done before the folding below, while the
   * evidence still names the instance. */
  auto machineNumbers =
      sem::instanceDictionaryName(sem::numClassName, sem::intTypeName);
  if (instancesBySymbol.find(machineNumbers) != instancesBySymbol.end()) {
    forEachNode([&](Ast &node) {
      auto *literal = dynamic_cast<AstInt *>(&node);
      if (!literal || !literal->fromInt ||
          literal->fromInt->evidence.size() != 1)
        return;

      auto &term = *literal->fromInt->evidence.front()->term;
      if (term.arguments.empty() && term.name == machineNumbers)
        literal->primitive = true;
    });
  }

  for (auto &pair : globalDefs.defsClass) {
    auto &declaration = *pair.second;
    for (auto &super : declaration.supers)
      selectorsBySymbol[sem::superSelectorName(
          declaration.getName(), super->className)] = {declaration.getName(),
                                                       super->className};
    for (auto &method : declaration.methods)
      selectorsBySymbol[sem::methodSelectorName(
          declaration.getName(), method->name)] = {declaration.getName(),
                                                   method->name};
  }

  /* A selection from a dictionary whose construction is right there is the
   * field itself. Applied from the inside out, so that a selection from a
   * selection is folded once the inner one has been. */
  std::function<std::shared_ptr<sem::EvidenceTerm>(const sem::EvidenceTerm &)>
      fold = [&](const sem::EvidenceTerm &term)
      -> std::shared_ptr<sem::EvidenceTerm> {
    if (term.parameter)
      return sem::EvidenceTerm::ofParameter(term.name);

    std::vector<std::shared_ptr<sem::EvidenceTerm>> arguments;
    for (auto &argument : term.arguments)
      arguments.push_back(fold(*argument));

    auto rebuilt =
        sem::EvidenceTerm::ofApplication(term.name, std::move(arguments));

    auto selector = selectorsBySymbol.find(term.name);
    if (selector == selectorsBySymbol.end() || rebuilt->arguments.size() != 1)
      return rebuilt;

    auto instanceIt = instancesBySymbol.find(rebuilt->arguments.front()->name);
    if (instanceIt == instancesBySymbol.end())
      return rebuilt;

    auto &instance = *instanceIt->second;
    if (instance.head->className != selector->second.first)
      return rebuilt;

    const sem::ClassInfo *info = classEnv.lookup(instance.head->className);
    assert(info != nullptr);

    /* The fields are the superclasses in order, then the methods in order,
     * which is what the selector was named after. */
    for (std::size_t i = 0; i < info->supers.size(); i++) {
      if (info->supers[i].className != selector->second.second)
        continue;

      return substituteTerm(*instance.dictionaryFields[i].evidence,
                            instance.dictionaryParams,
                            rebuilt->arguments.front()->arguments);
    }

    return rebuilt;
  };

  forEachEvidenceSlot([&](std::shared_ptr<sem::EvidenceSlot> &slot) {
    assert(slot->term != nullptr);
    slot->term = fold(*slot->term);
  });

  for (auto &instance : globalDefs.defsInstance) {
    for (auto &field : instance->dictionaryFields) {
      if (field.evidence)
        field.evidence = fold(*field.evidence);
    }
  }

  /* A method taken out of a dictionary whose construction is right there is
   * the function implementing it, called directly. The dictionary is then
   * needed only for whatever the implementation captured, which is a subset
   * of what the dictionary was built from. */
  forEachReference([&](AstLid &reference) {
    if (reference.lifted || reference.evidence.size() != 1)
      return;

    auto variable = reference.typeContext->lookup(reference.id);
    if (!variable || variable->visibility != sem::Visibility::Global ||
        !variable->mangledName)
      return;

    auto selector = selectorsBySymbol.find(*variable->mangledName);
    if (selector == selectorsBySymbol.end())
      return;

    /* Held on to, because the slot it lives in is about to be replaced. */
    auto term = reference.evidence.front()->term;
    auto instanceIt = instancesBySymbol.find(term->name);
    if (instanceIt == instancesBySymbol.end())
      return;

    auto &instance = *instanceIt->second;
    if (instance.head->className != selector->second.first)
      return;

    const sem::ClassInfo *info = classEnv.lookup(instance.head->className);
    assert(info != nullptr);

    for (std::size_t i = 0; i < info->methods.size(); i++) {
      if (info->methods[i].name != selector->second.second)
        continue;

      auto &field = instance.dictionaryFields[info->supers.size() + i];
      /* A method the instance left to the class's default is reached through
       * the dictionary the default was handed, which is the dictionary
       * itself, so there is nothing to shortcut. */
      if (!field.method)
        return;

      reference.id = field.method->mangledName;
      reference.lifted = true;
      reference.evidence.clear();

      /* The implementation takes what it captured, in the order lifting
       * prepended it, and each of those is one of the dictionaries the
       * instance was applied to. */
      for (auto &captured : field.method->capturedVariables) {
        auto argument =
            substituteTerm(*sem::EvidenceTerm::ofParameter(captured),
                           instance.dictionaryParams, term->arguments);

        std::shared_ptr<sem::EvidenceSlot> slot(new sem::EvidenceSlot());
        slot->term = std::move(argument);
        reference.evidence.push_back(std::move(slot));
      }
      return;
    }
  });

  /* A dictionary that reaches for nothing is the same dictionary wherever it
   * is written, so one of them is made and shared. Applied from the inside
   * out, so that the largest constant part of a term is the one named. */
  std::function<std::shared_ptr<sem::EvidenceTerm>(const sem::EvidenceTerm &)>
      intern = [&](const sem::EvidenceTerm &term)
      -> std::shared_ptr<sem::EvidenceTerm> {
    if (term.parameter)
      return sem::EvidenceTerm::ofParameter(term.name);

    if (isClosed(term)) {
      /* Already one node for the program: a ground instance's dictionary, or
       * a constant some other term was made to share. */
      if (term.arguments.empty())
        return sem::EvidenceTerm::ofApplication(term.name, {});

      auto key = termKey(term);
      auto known = dictionaryConstantsByTerm.find(key);
      if (known == dictionaryConstantsByTerm.end()) {
        std::string symbol = std::string("dict") + sem::generatedMarker +
                             std::to_string(dictionaryConstants.size());
        dictionaryConstants.push_back(std::unique_ptr<DictionaryConstant>(
            new DictionaryConstant(symbol, sem::EvidenceTerm::ofApplication(
                                               term.name, term.arguments))));
        known = dictionaryConstantsByTerm.emplace(key, symbol).first;
      }

      return sem::EvidenceTerm::ofApplication(known->second, {});
    }

    std::vector<std::shared_ptr<sem::EvidenceTerm>> arguments;
    for (auto &argument : term.arguments)
      arguments.push_back(intern(*argument));

    return sem::EvidenceTerm::ofApplication(term.name, std::move(arguments));
  };

  forEachEvidenceSlot([&](std::shared_ptr<sem::EvidenceSlot> &slot) {
    slot->term = intern(*slot->term);
  });

  for (auto &instance : globalDefs.defsInstance) {
    for (auto &field : instance->dictionaryFields) {
      if (field.evidence)
        field.evidence = intern(*field.evidence);
    }
  }
}

void Compiler::translate() {
  globalDefs.translate(globalScope);

  /* A default is a global of its own, named after the class and the method,
   * which no program can write and so nothing can collide with. */
  for (auto &pair : globalDefs.defsClass) {
    auto &declaration = *pair.second;
    for (auto &method : declaration.methods) {
      if (!method->body)
        continue;

      method->visibility = sem::Visibility::Global;
      method->mangledName =
          sem::defaultMethodName(declaration.getName(), method->name);
      method->translate(globalScope);
    }
  }

  /* An instance method is reached through the dictionary it is a field of,
   * never by name, so it is lifted like a local definition: it takes what it
   * captured, which is whatever dictionaries the instance holds under. */
  for (auto &instance : globalDefs.defsInstance) {
    for (auto &method : instance->methods) {
      method->visibility = sem::Visibility::Local;
      method->translate(globalScope);
    }
  }
}

void Compiler::compile() {
  for (auto &defDefn : globalDefs.defsDefn) {
    compileDefinition(*defDefn.second);
  }

  for (auto *definition : globalScope.getDefinitions()) {
    compileDefinition(*definition);
  }

  for (auto &pair : globalDefs.defsClass) {
    for (auto &method : pair.second->methods) {
      if (method->body)
        compileDefinition(*method);
    }
  }

  for (auto &instance : globalDefs.defsInstance) {
    instance->compile();
  }

  for (auto &constant : dictionaryConstants) {
    constant->compile();
  }
}

void Compiler::compileDefinition(DefinitionDefn &definition) {
  definition.compile();
}

Compiler::Compiler(const std::string &input, const std::string &output,
                   DumpKind dump)
    : fileManager(), globalDefs(), globalContext(new sem::TypeContext),
      classEnv(), mangler(), manager(), globalScope(mangler), generator(),
      inputFile(input), outputFile(output), objectFile("object.o"),
      dumpKind(dump) {
  addDefaultTypes();
  addDefaultFunctionTypes();
}

/* The supercombinator behind a binary operator: force both arguments, run
 * `operation` on them, and update the redex with what it left behind. */
void Compiler::createLLVMOperator(
    binop op, std::unique_ptr<ff::ir::Instruction> operation) {
  auto newFunction = generator.createCustomFunction(
      globalContext->getMangledName(opPrimitive(op)), 2);

  std::vector<std::unique_ptr<ff::ir::Instruction>> instructions;
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Push(1)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Eval()));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Push(1)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Eval()));
  instructions.push_back(std::move(operation));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(2)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Pop(2)));

  emitBuiltin(newFunction, instructions);
}

/* Fill in the body of a function the compiler wrote itself, the way a
 * definition of its own would have been filled in. */
void Compiler::emitBuiltin(
    llvm::Function *function,
    const std::vector<std::unique_ptr<ff::ir::Instruction>> &instructions) {
  generator.getBuilder().SetInsertPoint(&function->getEntryBlock());
  for (auto &instruction : instructions) {
    instruction->generate(generator, function);
  }

  generator.getBuilder().CreateRetVoid();
}

void Compiler::createLLVMBinop(binop op) {
  createLLVMOperator(
      op, std::unique_ptr<ff::ir::Instruction>(new ff::ir::Binop(op)));
}

void Compiler::createLLVMComparison(binop op) {
  createLLVMOperator(op,
                     std::unique_ptr<ff::ir::Instruction>(
                         new ff::ir::Compare(op, boolTrueTag, boolFalseTag)));
}

/* The supercombinator behind the composition operator, which is what a
 * `fun compose f g x = { f (g x) }` would have compiled to: build the
 * application graph and leave it in place of the redex. */
void Compiler::createLLVMCompose() {
  constexpr std::size_t arity = 3;
  auto newFunction = generator.createCustomFunction(
      globalContext->getMangledName(sem::composeName), arity);

  std::vector<std::unique_ptr<ff::ir::Instruction>> instructions;
  // g x, with the argument pushed before the function it is handed to.
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Push(2)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Push(2)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
  // f applied to what it built.
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Push(1)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(arity)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Pop(arity)));

  emitBuiltin(newFunction, instructions);
}

void Compiler::createLLVMListConstructors() {
  generateConstructorLLVM(generator, sem::listNilName, sem::listNilTag, 0);
  generateConstructorLLVM(generator, sem::listConsName, sem::listConsTag, 2);
}

void Compiler::generateLLVM() {
  createLLVMBinop(PLUS);
  createLLVMBinop(MINUS);
  createLLVMBinop(TIMES);
  createLLVMBinop(DIVIDE);
  for (auto &op : comparisonOps)
    createLLVMComparison(op);
  createLLVMCompose();
  createLLVMListConstructors();

  for (auto &defData : globalDefs.defsData) {
    defData.second->generateLLVM(generator);
  }

  /* A dictionary constructor and its selectors reach for nothing else, so
   * they are declared and filled in together, ahead of anything that names
   * one. */
  for (auto &pair : globalDefs.defsClass) {
    pair.second->generateLLVM(generator);
  }

  /* Every function must be declared before any body is generated: a body
   * reaches its callees by name, and lifting mixes the two sets freely. */
  for (auto &defDefn : globalDefs.defsDefn) {
    defDefn.second->declareLLVM(generator);
  }
  for (auto *definition : globalScope.getDefinitions()) {
    definition->declareLLVM(generator);
  }
  for (auto &pair : globalDefs.defsClass) {
    for (auto &method : pair.second->methods) {
      if (method->body)
        method->declareLLVM(generator);
    }
  }
  for (auto &instance : globalDefs.defsInstance) {
    instance->declareLLVM(generator);
  }
  for (auto &constant : dictionaryConstants) {
    constant->declareLLVM(generator);
  }

  /* An instance that holds under nothing builds one dictionary, the same one
   * every time, so it is allocated once and shared rather than rebuilt at
   * every mention of it. Marked before any body is generated, since it is
   * what a push of it compiles to that changes. */
  for (auto &instance : globalDefs.defsInstance) {
    if (instance->dictionaryParams.empty())
      generator.markAsCaf(instance->mangledName);
  }
  for (auto &constant : dictionaryConstants) {
    generator.markAsCaf(constant->mangledName);
  }

  for (auto &defDefn : globalDefs.defsDefn) {
    defDefn.second->generateLLVM(generator);
  }
  for (auto *definition : globalScope.getDefinitions()) {
    definition->generateLLVM(generator);
  }
  for (auto &pair : globalDefs.defsClass) {
    for (auto &method : pair.second->methods) {
      if (method->body)
        method->generateLLVM(generator);
    }
  }
  for (auto &instance : globalDefs.defsInstance) {
    instance->generateLLVM(generator);
  }
  for (auto &constant : dictionaryConstants) {
    constant->generateLLVM(generator);
  }

  generator.createCafInitializer();

  /* Everything the backend is about to be handed has to be well formed;
   * a mistake in what was generated is worth catching here rather than as
   * something inexplicable further down. */
  std::string verifierMessage;
  llvm::raw_string_ostream verifierStream(verifierMessage);
  if (llvm::verifyModule(generator.getModule(), &verifierStream))
    throw ff::CompilerError("the generated program is not well formed: " +
                            verifierMessage);

  generator.getModule().print(llvm::outs(), nullptr);
}

void Compiler::outputLLVM() {

  llvm::Triple targetTriple(llvm::sys::getDefaultTargetTriple());

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetAsmPrinter();

  std::string error;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(targetTriple, error);

  if (!target)
    throw ff::CompilerError(error);

  std::string cpu = "generic";
  std::string features = "";
  llvm::TargetOptions options;
  llvm::TargetMachine *targetMachine =
      target->createTargetMachine(targetTriple, cpu, features, options,
                                  std::optional<llvm::Reloc::Model>());

  generator.getModule().setDataLayout(targetMachine->createDataLayout());
  generator.getModule().setTargetTriple(targetTriple);

  std::error_code ec;
  llvm::raw_fd_ostream file(objectFile, ec, llvm::sys::fs::OF_None);
  if (ec)
    throw ff::CompilerError("could not open " + objectFile + " for writing");

  llvm::CodeGenFileType type = llvm::CodeGenFileType::ObjectFile;
  llvm::legacy::PassManager pm;
  if (targetMachine->addPassesToEmitFile(pm, file, NULL, type))
    throw ff::CompilerError("the target machine cannot emit an object file");

  pm.run(generator.getModule());
  file.close();
}

void Compiler::operator()() {
  /* The dumps that only need the program read stop before anything else
   * happens; the ones that need it checked run the front end first. */
  if (dumpKind == DumpKind::Ast || dumpKind == DumpKind::Source ||
      dumpKind == DumpKind::Classes) {
    dump();
    return;
  }

  parse();
  typecheck();

  if (dumpKind == DumpKind::Types) {
    printTypes();
    return;
  }

  if (dumpKind == DumpKind::Core) {
    globalDefs.print(0, std::cout);
    return;
  }

  translate();
  optimizeEvidence();
  compile();
  generateLLVM();
  outputLLVM();
  linkToRuntime();
  cleanUp();
}

FileManager &Compiler::getFileManager() { return this->fileManager; }

const sem::TypeManager &Compiler::getTypeManager() const {
  return this->manager;
}

void Compiler::linkToRuntime() {
  std::string command = "gcc -std=c11 -g -no-pie";
  for (const char *source : runtimeSources) {
    command += " ./runtime/";
    command += source;
  }
  command += " " + objectFile + " -o" + outputFile;

  if (std::system(command.c_str()) != 0) {
    throw ff::CompilerError("failed to link the program against the runtime");
  }
}

void Compiler::cleanUp() {
  std::string command = "rm " + objectFile;
  std::system(command.c_str());
}
} // namespace drv
} // namespace ff
