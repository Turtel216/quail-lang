#pragma once

#include <string>

namespace ff {
namespace drv {

/* What to write out instead of compiling. A dump reads the source file on its
 * own, without the prelude in front of it, so that what comes back describes
 * the program that was asked about and nothing else. */
enum class DumpKind {
  None,
  /* The tree as structure: what the program says, not how it was written. */
  Ast,
  /* The tree as source: what would parse back to the same structure. */
  Source,
  /* The classes and instances the program declares, once checked over. */
  Classes
};

class Cli {
public:
  std::string sourceFile;
  std::string outputFile = "a.out"; // Default output
  bool helpRequested = false;
  DumpKind dump = DumpKind::None;

  Cli(int argc, char *argv[]);

  void printUsage(const char *progName) const;

private:
  void parse(int argc, char *argv[]);
};

} // namespace drv
} // namespace ff
