#pragma once

#include "ast.hpp"
#include "file_manager.hpp"
#include "parser.hpp"
#include <optional>
#include <string>

namespace ff {
namespace drv {

/* Holds the state the scanner and parser need for one file, so that neither
 * has to reach for a global. The definitions and the file manager live
 * outside the driver and outlive it; the driver only writes into them. */
class ParseDriver {
private:
  std::string fileName;
  yy::location location;
  DefinitionGroup *globalDefs;
  FileManager *fileManager;
  SourceFile *file = nullptr;

  std::optional<std::string> errorMessage;
  yy::location errorLocation;

public:
  ParseDriver(FileManager &mgr, DefinitionGroup &defs, std::string _fileName)
      : fileName(std::move(_fileName)), globalDefs(&defs), fileManager(&mgr) {}

  /* Reads and parses the file, returning false if it could not be opened.
   * Throws a CompilerError if the file does not parse. */
  bool operator()();

  void reportError(const yy::location &loc, const std::string &message);

  inline yy::location &getLocation() noexcept { return this->location; }
  inline SourceFile &getFile() const noexcept { return *this->file; }
  inline DefinitionGroup &getGlobalDefs() const noexcept {
    return *this->globalDefs;
  }
};

} // namespace drv
} // namespace ff

#define YY_DECL                                                                \
  yy::parser::symbol_type yylex(yyscan_t yyscanner, ff::drv::ParseDriver &drv)
YY_DECL;
