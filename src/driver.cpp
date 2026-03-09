#include "../include/driver.hpp"
#include "../include/binop.hpp"
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
      throw 0;
    } else {
      llvm::CodeGenFileType type = llvm::CodeGenFileType::ObjectFile;
      llvm::legacy::PassManager pm;
      if (targetMachine->addPassesToEmitFile(pm, file, NULL, type)) {
        throw 0;
      } else {
        pm.run(generator.module);
        file.close();
      }
    }
  }
}

void generateLLVM(const std::vector<std::unique_ptr<Definition>> &prog,
                  const std::string &objectFile) {
  ff::cg::CodeGenerator generator;
  generateLLVMInternalOp(generator, PLUS);
  generateLLVMInternalOp(generator, MINUS);
  generateLLVMInternalOp(generator, TIMES);
  generateLLVMInternalOp(generator, DIVIDE);

  for (auto &definition : prog) {
    definition->generateLLVMFirst(generator);
  }

  for (auto &defintion : prog) {
    defintion->generateLLVMSecond(generator);
  }

  generator.module.print(llvm::outs(), nullptr);
  outputLLVM(generator, objectFile);
}

void linkToRuntime(const std::string &output) {
  std::string command = "gcc -no-pie ./runtime/runtime.c object.o -o" + output;
  std::system(command.c_str());
}

void cleanUp(const std::string &objectFile) {
  std::string command = "rm " + objectFile;
  std::system(command.c_str());
}

void typecheckProgram(const std::vector<std::unique_ptr<Definition>> &prog,
                      ff::sem::TypeManager &mgr, ff::sem::TypeContext &env) {
  std::shared_ptr<ff::sem::Type> int_type =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase("Int"));
  std::shared_ptr<ff::sem::Type> binop_type =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(
          int_type, std::shared_ptr<ff::sem::Type>(
                        new ff::sem::TypeArr(int_type, int_type))));

  env.bind("+", binop_type);
  env.bind("-", binop_type);
  env.bind("*", binop_type);
  env.bind("/", binop_type);

  for (auto &def : prog) {
    def->typeCheckFirst(mgr, env);
  }

  for (auto &def : prog) {
    def->typeCheckSecond(mgr, env);
  }

  for (auto &pair : env.getNames()) {
    std::cout << pair.first << ": ";
    pair.second->print(mgr, std::cout);
    std::cout << std::endl;
  }

  for (auto &def : prog) {
    def->resolve(mgr);
  }
}

void compileProgram(const std::vector<std::unique_ptr<Definition>> &prog) {
  for (auto &def : prog) {
    def->generate();

    DefinitionDefn *defn = dynamic_cast<DefinitionDefn *>(def.get());

    if (!defn)
      continue;

    for (auto &instruction : defn->instructions) {
      instruction->print(0, std::cout);
    }

    std::cout << std::endl;
  }
}

} // namespace drv
} // namespace ff
