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

namespace ff {
  namespace drv {

    void Compiler::addDefaultTypes() {
      globalContext->bindType("Int", std::unique_ptr<sem::Type>(new sem::TypeBase("Int")));
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
      globalContext->bind(sem::listNilName, std::move(nilScheme), sem::Visibility::Global);

      // Cons : forall a. a -> List a -> List a
      std::shared_ptr<sem::Type> consType(new sem::TypeArr(
          std::move(itemType),
          std::shared_ptr<sem::Type>(new sem::TypeArr(listOfItem, listOfItem))));
      std::shared_ptr<sem::TypeScheme> consScheme(new sem::TypeScheme(std::move(consType)));
      consScheme->forall.push_back(itemVar);
      globalContext->bind(sem::listConsName, std::move(consScheme), sem::Visibility::Global);
    }

    void Compiler::addBinopType(binop op, std::shared_ptr<sem::Type> type) {
      auto name = mangler.newMangledName(opAction(op));

      globalContext->bind(opName(op), std::move(type), sem::Visibility::Global);
      globalContext->setMangledName(opName(op), name);
    }

    void Compiler::addDefaultFunctionTypes() {

      std::shared_ptr<sem::Type> intType = globalContext->lookupType("Int");
    assert(intType != nullptr);
    std::shared_ptr<sem::Type> intTypeApp = std::shared_ptr<sem::Type>(new sem::TypeApp(intType));

    std::shared_ptr<sem::Type> closedIntOpType(
            new sem::TypeArr(intTypeApp, std::shared_ptr<sem::Type>(new sem::TypeArr(intTypeApp, intTypeApp))));

    constexpr binop closedOps[] = { PLUS, MINUS, TIMES, DIVIDE };
    for(auto& op : closedOps) addBinopType(op, closedIntOpType);
    }

    void Compiler::parseFile(const std::string& path) {
      ParseDriver driver(fileManager, globalDefs, path);
      if(!driver())
        throw CompilerError("could not open file " + path);
    }

    void Compiler::parse() {
      parseFile(preludePath);
      parseFile(inputFile);
    }

  void Compiler::typecheck() {
  std::set<std::string> freeVariables;
  globalDefs.findFree(manager, globalContext, ff::sem::Visibility::Global,
                   freeVariables);

  /* Nothing encloses the top level, so anything still free here that is not
   * already bound -- a constructor, an operator -- has no definition. */
  for (auto &free : freeVariables) {
    if (globalContext->lookup(free) == nullptr)
      throw ff::TypeError("undefined variable " + free);
  }

  globalDefs.typecheck(manager);

  for (auto &pair : globalContext->getNames()) {
    std::cout << pair.first << ": ";
    pair.second->scheme->print(manager, std::cout);
    std::cout << std::endl;
  }
    }

    void Compiler::translate() {
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

    Compiler::Compiler(const std::string& input, const std::string& output) : fileManager(), globalDefs(),
    globalContext(new sem::TypeContext), mangler(), manager(), globalScope(mangler), generator(), inputFile(input), outputFile(output),
    objectFile("object.o") {
      addDefaultTypes();
      addDefaultFunctionTypes();
    }

    void Compiler::createLLVMBinop(binop op) {
 auto newFunction =
      generator.createCustomFunction(globalContext->getMangledName(opName(op)), 2);

  std::vector<std::unique_ptr<ff::ir::Instruction>> instructions;
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Push(1)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Eval()));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Push(1)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Eval()));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Binop(op)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(2)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Pop(2)));

  generator.builder.SetInsertPoint(&newFunction->getEntryBlock());
  for (auto &instruction : instructions) {
    instruction->generate(generator, newFunction);
  }

  generator.builder.CreateRetVoid();
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

  generator.module.print(llvm::outs(), nullptr);
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

  generator.module.setDataLayout(targetMachine->createDataLayout());
  generator.module.setTargetTriple(targetTriple);

  std::error_code ec;
  llvm::raw_fd_ostream file(objectFile, ec, llvm::sys::fs::OF_None);
  if (ec)
    throw ff::CompilerError("could not open " + objectFile + " for writing");

  llvm::CodeGenFileType type = llvm::CodeGenFileType::ObjectFile;
  llvm::legacy::PassManager pm;
  if (targetMachine->addPassesToEmitFile(pm, file, NULL, type))
    throw ff::CompilerError("the target machine cannot emit an object file");

  pm.run(generator.module);
  file.close();
    }

    void Compiler::operator()() {
      parse();
      typecheck();
      translate();
      compile();
      generateLLVM();
      outputLLVM();
      linkToRuntime();
      cleanUp();
    }


        FileManager& Compiler::getFileManager() {
          return this->fileManager;
        }

        const sem::TypeManager& Compiler::getTypeManager() const {
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
  }
}
