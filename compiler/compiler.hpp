#pragma once

#include "ast.hpp"
#include "classes.hpp"
#include "cli.hpp"
#include "context.hpp"
#include "file_manager.hpp"
#include "generator.hpp"
#include "types.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ff {
  namespace drv {

class Compiler {
private:
  ff::drv::FileManager fileManager;
  DefinitionGroup globalDefs;
  std::shared_ptr<sem::TypeContext> globalContext;
  sem::ClassEnv classEnv;
  Mangler mangler;
  sem::TypeManager manager;
  GlobalScope globalScope;
  cg::CodeGenerator generator;
  std::string inputFile;
  std::string outputFile;
  /* The object file the backend writes and the linker consumes; it only
   * exists between generateLLVM and cleanUp. */
  std::string objectFile;
  /* The tags a comparison answers with, read off the Bool type the prelude
   * declares once it has been parsed. */
  int boolTrueTag = 0;
  int boolFalseTag = 0;
  DumpKind dumpKind;

  /* Which instance each generated dictionary function belongs to, and what
   * each selector selects, so that the optimizer can recognise a selection
   * from a dictionary it can see the construction of. */
  std::map<std::string, DefinitionInstance *> instancesBySymbol;
  std::map<std::string, std::pair<std::string, std::string>> selectorsBySymbol;
  /* The constant dictionaries the optimizer made, and the term each was made
   * out of, so that two of the same term share one. */
  std::vector<std::unique_ptr<DictionaryConstant>> dictionaryConstants;
  std::map<std::string, std::string> dictionaryConstantsByTerm;

        void addDefaultTypes();
        void addListType();
        void addBinopType(binop op, std::shared_ptr<sem::Type> type);
        void addDefaultFunctionTypes();
        void addComposeType();
        void addComparisonFunctionTypes();
        void parseFile(const std::string& path);
        void parse();
        void dump();
        void buildClassEnv();
        void typecheckDefaults();
        void typecheckInstances();
        void layOutDictionary(const sem::InstanceInfo &instance,
                              const sem::ClassInfo &info,
                              const std::shared_ptr<sem::Type> &headType,
                              const std::vector<sem::Given> &evidence);
        void resolveRemaining();
        void verifyEvidence();
        void optimizeEvidence();
        /* Every evidence term in the program, whatever holds it. */
        void forEachEvidenceSlot(
            const std::function<void(std::shared_ptr<sem::EvidenceSlot> &)> &visit);
        void forEachNode(const std::function<void(Ast &)> &visit);
        void forEachReference(const std::function<void(AstLid &)> &visit);
        void printTypes() const;
        void typecheck();
        void translate();
        void compileDefinition(DefinitionDefn &definition);
        void compile();
        void emitBuiltin(
            llvm::Function *function,
            const std::vector<std::unique_ptr<ir::Instruction>> &instructions);
        void createLLVMOperator(binop op,
                                std::unique_ptr<ir::Instruction> operation);
        void createLLVMBinop(binop op);
        void createLLVMComparison(binop op);
        void createLLVMCompose();
        void createLLVMListConstructors();
        void generateLLVM();
        void outputLLVM();
        void linkToRuntime();
        void cleanUp();

public:
        Compiler(const std::string &input, const std::string &output,
                 DumpKind dump = DumpKind::None);
        void operator()();
        FileManager& getFileManager();
        const sem::TypeManager& getTypeManager() const;
};

  }
}
