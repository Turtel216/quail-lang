#include "../include/cli.hpp"
#include "error.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace ff {
namespace drv {

Cli::Cli(int argc, char *argv[]) { parse(argc, argv); }

void Cli::print_usage(const char *prog_name) const {
  std::cout << "Usage: " << prog_name << " [source_file] [options]\n"
            << "Options:\n"
            << "  -o <path>      Specify the output file path\n"
            << "  --help         Display this information\n";
}

void Cli::parse(int argc, char *argv[]) {
  std::vector<std::string> args(argv + 1, argv + argc);

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string &arg = args[i];

    if (arg == "-h" || arg == "--help") {
      help_requested = true;
      return;
    } else if (arg == "-o") {
      if (i + 1 < args.size()) {
        output_file = args[++i];
      } else {
        throw CliError("Error: -o requires an output path.");
      }
    } else if (arg[0] == '-') {
      throw CliError("Unknown option: " + arg);
    } else {
      // If it doesn't start with '-', assume it's the source file
      if (source_file.empty()) {
        source_file = arg;
      } else {
        throw CliError("Error: Multiple source files provided.");
      }
    }
  }

  if (source_file.empty() && !help_requested) {
    throw CliError("Error: No input source file specified.");
  }
}
} // namespace drv
} // namespace ff
