#include "error.hpp"

namespace ff {

const char *CompilerError::what() const noexcept {
  return "an error occured while compiling the program";
}

void CompilerError::printAbout(std::ostream &to) const {
  to << what() << ": " << this->description << std::endl;
}

void CompilerError::printLocation(std::ostream &to, const drv::FileManager &fm,
                                  bool highlight) const {
  if (!this->loc)
    return;

  to << "occuring on line " << this->loc->begin.line << ":" << std::endl;
  fm.printLocation(to, *this->loc, highlight);
}

void CompilerError::prettyPrint(std::ostream &to,
                                const drv::FileManager &fm) const {
  printAbout(to);
  printLocation(to, fm, false);
}

const char *TypeError::what() const noexcept {
  return "an error occured while checking the types of the program";
}

void TypeError::prettyPrint(std::ostream &to,
                            const drv::FileManager &fm) const {
  printAbout(to);
  printLocation(to, fm, true);
}

void UnificationError::prettyPrint(std::ostream &to, const drv::FileManager &fm,
                                   const ff::sem::TypeManager &mgr) const {
  TypeError::prettyPrint(to, fm);

  to << "the expected type was:" << std::endl << "  \033[34m";
  left->print(mgr, to);
  to << std::endl << "\033[0mwhile the actual type was:" << std::endl;
  to << "  \033[32m";
  right->print(mgr, to);
  to << "\033[0m" << std::endl;
}

const char *CliError::what() const noexcept {
  return this->description.c_str();
}

} // namespace ff
