#include "../include/ast.hpp"
#include "../include/cli.hpp"
#include "../include/driver.hpp"
#include "../include/error.hpp"
#include "../include/types.hpp"
#include "parser.hpp"
#include <cstdio>

extern FILE *yyin;

extern void yypush_buffer_state(struct yy_buffer_state *);
extern void yypop_buffer_state();
extern struct yy_buffer_state *yy_create_buffer(FILE *, int);

void yy::parser::error(const std::string &msg) {
  std::cout << "An error occured: " << msg << std::endl;
}

extern std::map<std::string, std::unique_ptr<DefinitionData>> defsData;
extern std::map<std::string, std::unique_ptr<DefinitionDefn>> defsDefn;

constexpr std::string objectFile = "object.o";
constexpr const char *STD_LIB_PATH = "prelude/Base.ql";

int main(int argc, char *argv[]) {
  yy::parser parser;
  ff::sem::TypeManager mgr;
  std::shared_ptr<ff::sem::TypeContext> typeContext(new ff::sem::TypeContext);

  try {
    ff::drv::Cli cli(argc, argv);

    if (cli.helpRequested) {
      cli.printUsage(argv[0]);
      return 0;
    }

    FILE *file = fopen(cli.sourceFile.c_str(), "r");
    if (!file) {
      llvm::errs() << "Error: Could not open file" << cli.sourceFile << '\n';
    }

    // Open the standard library file
    FILE *stdlibFile = fopen(STD_LIB_PATH, "r");
    if (!stdlibFile) {
      llvm::errs() << "Error: Could not open standard library file\n";
      fclose(file);
      return 1;
    }

    yyin = file;
    yypush_buffer_state(yy_create_buffer(yyin, 16384));

    yyin = stdlibFile;
    yypush_buffer_state(yy_create_buffer(yyin, 16384));

    parser.parse();

    for (auto &defDefn : defsDefn) {
      std::cout << defDefn.second->name;
      for (auto &param : defDefn.second->params)
        std::cout << " " << param;
      std::cout << ":" << std::endl;

      defDefn.second->body->print(1, std::cout);
    }

    ff::drv::typecheckProgram(defsData, defsDefn, mgr, typeContext);
    ff::drv::compileProgram(defsDefn);
    ff::drv::generateLLVM(defsData, defsDefn,
                          objectFile); // TODO: Fix hardcoded output file
    ff::drv::linkToRuntime(cli.outputFile);
    ff::drv::cleanUp(objectFile); // TODO: Fix hardcoded output file
  } catch (const ff::UnificationError &err) {
    std::cout << "failed to unify types: " << std::endl;
    std::cout << "  (1) \033[34m";
    err.left->print(mgr, std::cout);

    std::cout << "\033[0m" << std::endl;
    std::cout << "  (2) \033[32m";
    err.right->print(mgr, std::cout);

    std::cout << "\033[0m" << std::endl;
  } catch (const ff::TypeError &err) {
    std::cout << "failed to type check program: " << err.description
              << std::endl;
  } catch (const ff::CliError &err) {
    std::cout << err.what();
  } catch (const ff::DebugError &err) {
    std::cout << err.what();
  }
}
