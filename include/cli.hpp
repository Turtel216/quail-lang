#pragma once

#include <string>

namespace ff {
namespace drv {

class Cli {
public:
  std::string sourceFile;
  std::string outputFile = "a.out"; // Default output
  bool helpRequested = false;

  Cli(int argc, char *argv[]);

  void printUsage(const char *progName) const;

private:
  void parse(int argc, char *argv[]);
};

} // namespace drv
} // namespace ff
