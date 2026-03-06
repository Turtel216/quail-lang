#pragma once

#include <string>

namespace ff {
namespace drv {

class Cli {
public:
  std::string source_file;
  std::string output_file = "output.o"; // Default output
  bool compile_only = false;            // -c flag
  bool help_requested = false;

  Cli(int argc, char *argv[]);

  void print_usage(const char *prog_name) const;

private:
  void parse(int argc, char *argv[]);
};

} // namespace drv
} // namespace ff
