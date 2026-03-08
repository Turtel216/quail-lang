#pragma once

#include "ast.hpp"
#include <string>

namespace ff {
namespace drv {
void generateLLVM(const std::vector<std::unique_ptr<Definition>> &prg,
                  const std::string &output_file);
void linkToRuntime(const std::string &output);
void cleanUp(const std::string &objectFile);
} // namespace drv
} // namespace ff
