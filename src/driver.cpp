#include "../include/driver.hpp"
#include "../include/binop.hpp"
#include "error.hpp"
#include "graph_function.hpp"
#include "types.hpp"
#include <cstdlib>
#include <iostream>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <optional>

namespace ff {
namespace drv {
void generateLLVMInternalOp(ff::cg::CodeGenerator &generator, binop op) {
  auto newFunction = generator.createCustomFunction(opAction(op), 2);

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

void outputLLVM(ff::cg::CodeGenerator &generator,
                const std::string &objectFile) {
  llvm::Triple targetTriple(llvm::sys::getDefaultTargetTriple());

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetAsmPrinter();

  std::string error;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(targetTriple, error);

  if (!target) {
    std::cerr << error << std::endl;
  } else {
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
    if (ec) {
      throw ff::CompilerError("could not open " + objectFile + " for writing");
    } else {
      llvm::CodeGenFileType type = llvm::CodeGenFileType::ObjectFile;
      llvm::legacy::PassManager pm;
      if (targetMachine->addPassesToEmitFile(pm, file, NULL, type)) {
        throw ff::CompilerError(
            "the target machine cannot emit an object file");
      } else {
        pm.run(generator.module);
        file.close();
      }
    }
  }
}

void generateLLVM(DefinitionGroup &program, const GlobalScope &scope,
                  const std::string &outputFile) {
  ff::cg::CodeGenerator generator;
  generateLLVMInternalOp(generator, PLUS);
  generateLLVMInternalOp(generator, MINUS);
  generateLLVMInternalOp(generator, TIMES);
  generateLLVMInternalOp(generator, DIVIDE);

  for (auto &defData : program.defsData) {
    defData.second->generateLLVM(generator);
  }

  /* Every function must be declared before any body is generated: a body
   * reaches its callees by name, and lifting mixes the two sets freely. */
  for (auto &defDefn : program.defsDefn) {
    defDefn.second->declareLLVM(generator);
  }
  for (auto *definition : scope.getDefinitions()) {
    definition->declareLLVM(generator);
  }

  for (auto &defDefn : program.defsDefn) {
    defDefn.second->generateLLVM(generator);
  }
  for (auto *definition : scope.getDefinitions()) {
    definition->generateLLVM(generator);
  }

  generator.module.print(llvm::outs(), nullptr);
  outputLLVM(generator, outputFile);
}

/* Translation units making up the runtime, compiled fresh on every link.
 * Keep in sync with the contents of runtime/ -- runtime.h documents how the
 * pieces fit together. */
static const char *const runtimeSources[] = {
    "eval.c", "gc.c",    "gmachine.c", "heap.c",
    "main.c", "panic.c", "stack.c",    "vec.c",
};

void linkToRuntime(const std::string &output) {
  std::string command = "gcc -std=c11 -g -no-pie";
  for (const char *source : runtimeSources) {
    command += " ./runtime/";
    command += source;
  }
  command += " object.o -o" + output;

  if (std::system(command.c_str()) != 0) {
    throw ff::CompilerError("failed to link the program against the runtime");
  }
}

void cleanUp(const std::string &objectFile) {
  std::string command = "rm " + objectFile;
  std::system(command.c_str());
}

void typecheckProgram(DefinitionGroup &program, ff::sem::TypeManager &mgr,
                      std::shared_ptr<ff::sem::TypeContext> &typeContext) {
  auto intType = std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase("Int"));
  typeContext->bindType("Int", intType);
  std::shared_ptr<ff::sem::Type> intTypeApp =
      std::shared_ptr<ff::sem::Type>(new sem::TypeApp(intType));

  auto binopType = std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(
      intTypeApp, std::shared_ptr<ff::sem::Type>(
                      new ff::sem::TypeArr(intTypeApp, intTypeApp))));

  typeContext->bind("+", binopType, ff::sem::Visibility::Global);
  typeContext->bind("-", binopType, ff::sem::Visibility::Global);
  typeContext->bind("*", binopType, ff::sem::Visibility::Global);
  typeContext->bind("/", binopType, ff::sem::Visibility::Global);

  std::set<std::string> freeVariables;
  program.findFree(mgr, typeContext, ff::sem::Visibility::Global,
                   freeVariables);

  /* Nothing encloses the top level, so anything still free here that is not
   * already bound -- a constructor, an operator -- has no definition. */
  for (auto &free : freeVariables) {
    if (typeContext->lookup(free) == nullptr)
      throw ff::TypeError("undefined variable " + free);
  }

  program.typecheck(mgr);

  for (auto &pair : typeContext->getNames()) {
    std::cout << pair.first << ": ";
    pair.second->scheme->print(mgr, std::cout);
    std::cout << std::endl;
  }
}

void translateProgram(DefinitionGroup &program, GlobalScope &scope) {
  program.translate(scope);
}

namespace {
void compileDefinition(DefinitionDefn &definition) {
  definition.compile();

  for (auto &instruction : definition.instructions) {
    instruction->print(0, std::cout);
  }

  std::cout << std::endl;
}
} // namespace

void compileProgram(DefinitionGroup &program, const GlobalScope &scope) {
  for (auto &defDefn : program.defsDefn) {
    compileDefinition(*defDefn.second);
  }

  for (auto *definition : scope.getDefinitions()) {
    compileDefinition(*definition);
  }
}

} // namespace drv
} // namespace ff
