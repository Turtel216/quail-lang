#include "../include/error.hpp"

namespace ff {

const char *TypeError::what() const noexcept {
  return this->description.c_str();
}

const char *CliError::what() const noexcept {
  return this->description.c_str();
}

const char *DebugError::what() const noexcept {
  return this->description.c_str();
}

} // namespace ff
