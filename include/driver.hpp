#pragma once

#include "ast.hpp"
#include "context.hpp"
#include "types.hpp"
#include <string>

namespace ff {
namespace drv {

void compileProgram(const std::vector<std::unique_ptr<Definition>> &prog);
void typecheckProgram(const std::vector<std::unique_ptr<Definition>> &prog,
                      ff::sem::TypeManager &mgr, ff::sem::TypeContext &env);
void generateLLVM(const std::vector<std::unique_ptr<Definition>> &prg,
                  const std::string &output_file);
void linkToRuntime(const std::string &output);
void cleanUp(const std::string &objectFile);
} // namespace drv
} // namespace ff
