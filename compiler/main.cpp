#include "cli.hpp"
#include "compiler.hpp"
#include "error.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
  try {
    ff::drv::Cli cli(argc, argv);

    if (cli.helpRequested) {
      cli.printUsage(argv[0]);
      return 0;
    }

    ff::drv::Compiler comp(cli.sourceFile, cli.outputFile);

    try {
      comp();
    } catch(const ff::UnificationError& err) {
      err.prettyPrint(std::cerr, comp.getFileManager(), comp.getTypeManager());
      return 1;
    } catch(const ff::TypeError& err) {
      err.prettyPrint(std::cerr, comp.getFileManager());
      return 1;
    } catch(const ff::CompilerError& err) {
      err.prettyPrint(std::cerr, comp.getFileManager());
      return 1;
    }
} catch(const ff::CliError &err) {
  std::cerr <<err.what() << std::endl;
  return 1;
}

  return 0;
}
