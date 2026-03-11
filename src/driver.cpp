#include "../include/driver.hpp"
#include "../include/binop.hpp"
#include "graph_function.hpp"
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

void generateLLVM(
    const std::map<std::string, std::unique_ptr<DefinitionData>> &defsData,
    const std::map<std::string, std::unique_ptr<DefinitionDefn>> &defsDefn,
    const std::string &output_file) {
  ff::cg::CodeGenerator generator;
  generateLLVMInternalOp(generator, PLUS);
  generateLLVMInternalOp(generator, MINUS);
  generateLLVMInternalOp(generator, TIMES);
  generateLLVMInternalOp(generator, DIVIDE);

  for (auto &defData : defsData) {
    defData.second->generateLLVM(generator);
  }

  for (auto &defDefn : defsDefn) {
    defDefn.second->generateLLVM(generator);
  }

  generator.module.print(llvm::outs(), nullptr);
  outputLLVM(generator, output_file);
}

void linkToRuntime(const std::string &output) {
  std::string command = "gcc -no-pie ./runtime/runtime.c object.o -o" + output;
  std::system(command.c_str());
}

void cleanUp(const std::string &objectFile) {
  std::string command = "rm " + objectFile;
  std::system(command.c_str());
}

void typecheckProgram(
    const std::map<std::string, std::unique_ptr<DefinitionData>> &defsData,
    const std::map<std::string, std::unique_ptr<DefinitionDefn>> &defsDefn,
    ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeContext) {
  auto intType = std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase("Int"));
  typeContext->bindType("Int", intType);

  auto binopType = std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(
      intType,
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(intType, intType))));

  typeContext->bind("+", binopType);
  typeContext->bind("-", binopType);
  typeContext->bind("*", binopType);
  typeContext->bind("/", binopType);

  for (auto &defData : defsData) {
    defData.second->insertTypes(mgr, typeContext);
  }
  for (auto &defData : defsData) {
    defData.second->insertConstructors();
  }

  ff::sem::FunctionGraph dependencyGraph;

  for (auto &defDefn : defsDefn) {
    defDefn.second->findFree(mgr, typeContext);
    dependencyGraph.addFunction(defDefn.second->name);

    for (auto &dependency : defDefn.second->freeVariables) {
      if (defsDefn.find(dependency) == defsDefn.end())
        throw 0;

      dependencyGraph.addEdge(defDefn.second->name, dependency);
    }
  }

  std::vector<std::unique_ptr<ff::sem::Group>> groups =
      dependencyGraph.computeOrder();

  for (auto it = groups.rbegin(); it != groups.rend(); it++) {
    auto &group = *it;
    for (auto &defDefnName : group->members) {
      auto &defDefn = defsDefn.find(defDefnName)->second;
      defDefn->insertTypes(mgr);
    }

    for (auto &defDefnName : group->members) {
      auto &defDefn = defsDefn.find(defDefnName)->second;
      defDefn->typecheck(mgr);
    }

    for (auto &def_defnn_name : group->members) {
      typeContext->generalize(def_defnn_name, mgr);
    }
  }

  for (auto &pair : typeContext->getNames()) {
    std::cout << pair.first << ": ";
    pair.second->print(mgr, std::cout);
    std::cout << std::endl;
  }
}

void compileProgram(
    const std::map<std::string, std::unique_ptr<DefinitionDefn>> &defsDefn) {
  for (auto &defDefn : defsDefn) {
    defDefn.second->compile();

    for (auto &instruction : defDefn.second->instructions) {
      instruction->print(0, std::cout);
    }

    std::cout << std::endl;
  }
}

} // namespace drv
} // namespace ff
