#include "parse_driver.hpp"
#include "error.hpp"
#include "scanner.hpp"
#include <cstdio>

namespace ff {
namespace drv {

/* The first complaint is the one worth keeping: a stray character the
 * scanner names precisely is more useful than the syntax error the parser
 * trips over a token later. */
void ParseDriver::reportError(const yy::location &loc,
                              const std::string &message) {
  if (errorMessage)
    return;

  errorMessage = message;
  errorLocation = loc;
}

bool ParseDriver::operator()() {
  FILE *stream = fopen(fileName.c_str(), "r");
  if (!stream)
    return false;

  file = &fileManager->open(fileName);
  location.initialize(file->getNamePointer());

  yyscan_t scanner;
  yylex_init(&scanner);
  yyset_in(stream, scanner);

  yy::parser parser(scanner, *this);
  parser();

  yylex_destroy(scanner);
  fclose(stream);

  /* The recorded text has to be handed over before anyone can quote it,
   * including the error we may be about to throw. */
  file->finalize();

  if (errorMessage)
    throw ff::CompilerError(*errorMessage, errorLocation);

  return true;
}

} // namespace drv
} // namespace ff

void yy::parser::error(const location_type &loc, const std::string &message) {
  drv.reportError(loc, message);
}
