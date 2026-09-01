#include "ast.hpp"
#include "cli.hpp"
#include "driver.hpp"
#include "error.hpp"
#include "file_manager.hpp"
#include "parse_driver.hpp"
#include "types.hpp"
#include <iostream>

constexpr std::string objectFile = "object.o";
constexpr const char *STD_LIB_PATH = "prelude/Base.ql";

namespace {

void parseFile(ff::drv::FileManager &files, DefinitionGroup &program,
               const std::string &path) {
  ff::drv::ParseDriver driver(files, program, path);
  if (!driver())
    throw ff::CompilerError("could not open file " + path);
}

} // namespace

int main(int argc, char *argv[]) {
  ff::drv::FileManager files;
  ff::sem::TypeManager mgr;
  DefinitionGroup program;
  std::shared_ptr<ff::sem::TypeContext> typeContext(new ff::sem::TypeContext);

  try {
    ff::drv::Cli cli(argc, argv);

    if (cli.helpRequested) {
      cli.printUsage(argv[0]);
      return 0;
    }

    parseFile(files, program, STD_LIB_PATH);
    parseFile(files, program, cli.sourceFile);

    for (auto &defDefn : program.defsDefn) {
      std::cout << defDefn.second->name;
      for (auto &param : defDefn.second->params)
        std::cout << " " << param;
      std::cout << ":" << std::endl;

      defDefn.second->body->print(1, std::cout);
    }

    GlobalScope globalScope;

    ff::drv::typecheckProgram(program, mgr, typeContext);
    ff::drv::translateProgram(program, globalScope);
    ff::drv::compileProgram(program, globalScope);
    ff::drv::generateLLVM(program, globalScope,
                          objectFile); // TODO: Fix hardcoded output file
    ff::drv::linkToRuntime(cli.outputFile);
    ff::drv::cleanUp(objectFile); // TODO: Fix hardcoded output file
  } catch (const ff::UnificationError &err) {
    err.prettyPrint(std::cerr, files, mgr);
    return 1;
  } catch (const ff::CompilerError &err) {
    err.prettyPrint(std::cerr, files);
    return 1;
  } catch (const ff::CliError &err) {
    std::cerr << err.what() << std::endl;
    return 1;
  }
}
