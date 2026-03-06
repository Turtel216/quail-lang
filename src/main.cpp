#include "../include/ast.hpp"
#include "../include/cli.hpp"
#include "../include/error.hpp"
#include "../include/types.hpp"
#include "binop.hpp"
#include "generator.hpp"
#include "instructions.hpp"
#include "parser.hpp"
#include <cstdio>
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
#include <stdexcept>

extern FILE *yyin;

void yy::parser::error(const std::string &msg) {
  std::cout << "An error occured: " << msg << std::endl;
}

extern std::vector<std::unique_ptr<Definition>> program;

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

  for (auto &pair : env.names) {
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

void outputLLVM(ff::cg::CodeGenerator &generator, const std::string &filename) {
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
    llvm::raw_fd_ostream file(filename, ec, llvm::sys::fs::OF_None);
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
                  const std::string &output_file) {
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
  outputLLVM(generator, output_file);
}

int main(int argc, char *argv[]) {
  yy::parser parser;
  ff::sem::TypeManager mgr;
  ff::sem::TypeContext env;

  try {
    ff::drv::Cli cli(argc, argv);

    if (cli.help_requested) {
      cli.print_usage(argv[0]);
      return 0;
    }

    FILE *file = fopen(cli.source_file.c_str(), "r");
    if (!file) {
      llvm::errs() << "Error: Could not open file" << cli.source_file << '\n';
    }

    yyin = file;

    parser.parse();
    for (auto &definition : program) {
      DefinitionDefn *def = dynamic_cast<DefinitionDefn *>(definition.get());
      if (!def)
        continue;

      std::cout << def->name;
      for (auto &param : def->params)
        std::cout << " " << param;
      std::cout << ":" << std::endl;

      def->body->print(1, std::cout);
    }

    typecheckProgram(program, mgr, env);
    compileProgram(program);
    generateLLVM(program, cli.output_file);
  } catch (ff::UnificationError &err) {
    std::cout << "failed to unify types: " << std::endl;
    std::cout << "  (1) \033[34m";
    err.left->print(mgr, std::cout);

    std::cout << "\033[0m" << std::endl;
    std::cout << "  (2) \033[32m";
    err.right->print(mgr, std::cout);

    std::cout << "\033[0m" << std::endl;
  } catch (ff::TypeError &err) {
    std::cout << "failed to type check program: " << err.description
              << std::endl;
  } catch (std::runtime_error &err) {
    std::cout << err.what();
  }
}
