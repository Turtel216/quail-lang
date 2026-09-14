#include "compiler.hpp"
#include "ast.hpp"
#include "binop.hpp"
#include "context.hpp"
#include "error.hpp"
#include "parse_driver.hpp"
#include "types.hpp"
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
    "eval.c", "gc.c",    "gmachine.c", "heap.c",
    "main.c", "panic.c", "stack.c",    "vec.c",
};

/* Definitions every program gets for free, parsed ahead of the source file
 * so that anything it declares can be redefined. */
static const char *const preludePath = "prelude/Base.ql";

/* The operators that answer with a Bool rather than an Int. They are set up
 * apart from the arithmetic ones because Bool comes from the prelude. */
static constexpr binop comparisonOps[] = {EQUALS,  NOTEQUALS, LESS,
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

void Compiler::addBinopType(binop op, std::shared_ptr<sem::Type> type) {
  auto name = mangler.newMangledName(opAction(op));

  globalContext->bind(opName(op), std::move(type), sem::Visibility::Global);
  globalContext->setMangledName(opName(op), name);
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
    if (globalContext->lookup(free) == nullptr)
      throw ff::TypeError("undefined variable " + free);
  }

  globalDefs.typecheck(manager);
  typecheckInstances();
  resolveRemaining();
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

    /* What the instance was allowed to assume. */
    std::vector<sem::Pred> given;
    for (auto &pred : instance->qual.preds)
      given.push_back(
          sem::Pred(pred.className, manager.substitute(subst, pred.type)));

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

      method->findFree(manager, globalContext);
      for (auto &free : method->freeVariables) {
        if (globalContext->lookup(free) == nullptr)
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

      for (auto &one :
           classEnv.reduce(manager, manager.takeWantedFrom(mark), given)) {
        if (classEnv.entail(manager, given, one.pred))
          continue;

        sem::TypeNamer namer;
        std::ostringstream errorStream;
        errorStream << "the method " << method->name << " needs ";
        sem::printReadable(manager, one.pred, namer, errorStream);
        errorStream << ", which the instance does not hold under";

        throw ff::TypeError(errorStream.str(), one.loc);
      }
    }
  }
}

/* Constraints that reached the top level unanswered. Nothing encloses it, so
 * each is settled by defaulting or is a mistake. */
void Compiler::resolveRemaining() {
  auto leftover = classEnv.reduce(manager, manager.takeWantedFrom(0));
  if (leftover.empty())
    return;

  std::vector<sem::Pred> preds;
  std::set<std::string> vars;
  for (auto &one : leftover) {
    preds.push_back(one.pred);
    manager.findFree(one.pred.type, vars);
  }

  for (auto &var : vars)
    classEnv.defaultVariable(manager, var, preds);

  leftover = classEnv.reduce(manager, std::move(leftover));
  if (leftover.empty())
    return;

  sem::TypeNamer namer;
  std::ostringstream errorStream;
  errorStream << "nothing settles ";
  sem::printReadable(manager, leftover.front().pred, namer, errorStream);

  throw ff::TypeError(errorStream.str(), leftover.front().loc);
}

void Compiler::printTypes() const {
  for (auto &pair : globalContext->getNames()) {
    std::cout << pair.first << " : ";
    sem::printReadable(manager, *pair.second->scheme, std::cout);
    std::cout << std::endl;
  }
}

void Compiler::translate() {
  /* Dictionary passing is not built yet, so a program that declares a class
   * typechecks but has nothing to run. Reported here rather than left to
   * fail as a missing symbol further down. TODO: remove once instances are
   * lowered. */
  if (!classEnv.isEmpty())
    throw ff::CompilerError(
        "type classes are checked but not yet compiled; a program that "
        "declares one cannot be built yet");

  globalDefs.translate(globalScope);
}

void Compiler::compile() {
  for (auto &defDefn : globalDefs.defsDefn) {
    compileDefinition(*defDefn.second);
  }

  for (auto *definition : globalScope.getDefinitions()) {
    compileDefinition(*definition);
  }
}

void Compiler::compileDefinition(DefinitionDefn &definition) {
  definition.compile();
}

Compiler::Compiler(const std::string &input, const std::string &output,
                   DumpKind dump)
    : fileManager(), globalDefs(), globalContext(new sem::TypeContext),
      classEnv(), mangler(), manager(), globalScope(mangler), generator(),
      inputFile(input),
      outputFile(output), objectFile("object.o"), dumpKind(dump) {
  addDefaultTypes();
  addDefaultFunctionTypes();
}

/* The supercombinator behind a binary operator: force both arguments, run
 * `operation` on them, and update the redex with what it left behind. */
void Compiler::createLLVMOperator(
    binop op, std::unique_ptr<ff::ir::Instruction> operation) {
  auto newFunction = generator.createCustomFunction(
      globalContext->getMangledName(opName(op)), 2);

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
  createLLVMOperator(op, std::unique_ptr<ff::ir::Instruction>(
                             new ff::ir::Compare(op, boolTrueTag,
                                                 boolFalseTag)));
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

  /* Every function must be declared before any body is generated: a body
   * reaches its callees by name, and lifting mixes the two sets freely. */
  for (auto &defDefn : globalDefs.defsDefn) {
    defDefn.second->declareLLVM(generator);
  }
  for (auto *definition : globalScope.getDefinitions()) {
    definition->declareLLVM(generator);
  }

  for (auto &defDefn : globalDefs.defsDefn) {
    defDefn.second->generateLLVM(generator);
  }
  for (auto *definition : globalScope.getDefinitions()) {
    definition->generateLLVM(generator);
  }

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

  translate();
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
