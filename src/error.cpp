#include "../include/error.hpp"

namespace ff {

const char *TypeError::what() const noexcept {
  return "an error occured type checking";
}

} // namespace ff
