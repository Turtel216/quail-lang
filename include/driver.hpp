#pragma once

#include "ast.hpp"
#include "context.hpp"
#include "types.hpp"
#include <string>

namespace ff {
namespace drv {

void typecheckProgram(DefinitionGroup &program, ff::sem::TypeManager &mgr,
                      std::shared_ptr<ff::sem::TypeContext> &typeContext);
void translateProgram(DefinitionGroup &program, GlobalScope &scope);
void compileProgram(DefinitionGroup &program, const GlobalScope &scope);
void generateLLVM(DefinitionGroup &program, const GlobalScope &scope,
                  const std::string &outputFile);
void linkToRuntime(const std::string &output);
void cleanUp(const std::string &objectFile);
} // namespace drv
} // namespace ff
