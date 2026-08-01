#pragma once

#include "ast.hpp"
#include "context.hpp"
#include "types.hpp"
#include <string>

namespace ff {
namespace drv {

void compileProgram(
    const std::map<std::string, std::unique_ptr<DefinitionDefn>> &defsDefn);
void typecheckProgram(
    const std::map<std::string, std::unique_ptr<DefinitionData>> &defsData,
    const std::map<std::string, std::unique_ptr<DefinitionDefn>> &defsDefn,
    ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeContext);
void generateLLVM(
    const std::map<std::string, std::unique_ptr<DefinitionData>> &defsData,
    const std::map<std::string, std::unique_ptr<DefinitionDefn>> &defsDefn,
    const std::string &outputFile);
void linkToRuntime(const std::string &output);
void cleanUp(const std::string &objectFile);
} // namespace drv
} // namespace ff
