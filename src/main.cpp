#include "../include/ast.hpp"
#include "../include/cli.hpp"
#include "../include/driver.hpp"
#include "../include/error.hpp"
#include "../include/types.hpp"
#include "parser.hpp"
#include <cstdio>

extern FILE *yyin;

void yy::parser::error(const std::string &msg) {
  std::cout << "An error occured: " << msg << std::endl;
}

extern std::map<std::string, std::unique_ptr<DefinitionData>> defs_data;
extern std::map<std::string, std::unique_ptr<DefinitionDefn>> defs_defn;

constexpr std::string objectFile = "object.o";

int main(int argc, char *argv[]) {
  yy::parser parser;
  ff::sem::TypeManager mgr;
  std::shared_ptr<ff::sem::TypeContext> typeContext(new ff::sem::TypeContext);

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
    for (auto &defDefn : defs_defn) {
      std::cout << defDefn.second->name;
      for (auto &param : defDefn.second->params)
        std::cout << " " << param;
      std::cout << ":" << std::endl;

      defDefn.second->body->print(1, std::cout);
    }

    std::cout << "Type checking\n";
    ff::drv::typecheckProgram(defs_data, defs_defn, mgr, typeContext);
    std::cout << "Compiling\n";
    ff::drv::compileProgram(defs_defn);
    std::cout << "LLVM\n";
    ff::drv::generateLLVM(defs_data, defs_defn,
                          objectFile); // TODO: Fix hardcoded output file
    ff::drv::linkToRuntime(cli.output_file);
    ff::drv::cleanUp(objectFile); // TODO: Fix hardcoded output file
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
  } catch (ff::CliError &err) {
    std::cout << err.what();
  }
}
