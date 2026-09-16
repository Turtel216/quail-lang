#include "cli.hpp"
#include "error.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace ff {
namespace drv {

Cli::Cli(int argc, char *argv[]) { parse(argc, argv); }

void Cli::printUsage(const char *progName) const {
  std::cout << "Usage: " << progName << " [source_file] [options]\n"
            << "Options:\n"
            << "  -o <path>      Specify the output file path\n"
            << "  --dump-ast     Print the structure of the parsed program\n"
            << "  --dump-source  Print the parsed program back out as source\n"
            << "  --check-classes Check the classes and instances and print them\n"
            << "  --dump-types   Print the type inferred for every global\n"
            << "  --dump-core    Print the program with its dictionaries made explicit\n"
            << "  --help         Display this information\n";
}

void Cli::parse(int argc, char *argv[]) {
  std::vector<std::string> args(argv + 1, argv + argc);

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string &arg = args[i];

    if (arg == "-h" || arg == "--help") {
      helpRequested = true;
      return;
    } else if (arg == "--dump-ast") {
      dump = DumpKind::Ast;
    } else if (arg == "--dump-source") {
      dump = DumpKind::Source;
    } else if (arg == "--check-classes") {
      dump = DumpKind::Classes;
    } else if (arg == "--dump-types") {
      dump = DumpKind::Types;
    } else if (arg == "--dump-core") {
      dump = DumpKind::Core;
    } else if (arg == "-o") {
      if (i + 1 < args.size()) {
        outputFile = args[++i];
      } else {
        throw CliError("Error: -o requires an output path.");
      }
    } else if (arg[0] == '-') {
      throw CliError("Unknown option: " + arg);
    } else {
      // If it doesn't start with '-', assume it's the source file
      if (sourceFile.empty()) {
        sourceFile = arg;
      } else {
        throw CliError("Error: Multiple source files provided.");
      }
    }
  }

  if (sourceFile.empty() && !helpRequested) {
    throw CliError("Error: No input source file specified.");
  }
}
} // namespace drv
} // namespace ff
