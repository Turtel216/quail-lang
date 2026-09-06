#pragma once

#include "ast.hpp"
#include "context.hpp"
#include "file_manager.hpp"
#include "generator.hpp"
#include "types.hpp"
#include <memory>
#include <string>

namespace ff {
  namespace drv {

class Compiler {
private:
  ff::drv::FileManager fileManager;
  DefinitionGroup globalDefs;
  std::shared_ptr<sem::TypeContext> globalContext;
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

        void addDefaultTypes();
        void addListType();
        void addBinopType(binop op, std::shared_ptr<sem::Type> type);
        void addDefaultFunctionTypes();
        void addComparisonFunctionTypes();
        void parseFile(const std::string& path);
        void parse();
        void typecheck();
        void translate();
        void compileDefinition(DefinitionDefn &definition);
        void compile();
        void createLLVMOperator(binop op,
                                std::unique_ptr<ir::Instruction> operation);
        void createLLVMBinop(binop op);
        void createLLVMComparison(binop op);
        void createLLVMListConstructors();
        void generateLLVM();
        void outputLLVM();
        void linkToRuntime();
        void cleanUp();

public:
        Compiler(const std::string& input, const std::string& output);
        void operator()();
        FileManager& getFileManager();
        const sem::TypeManager& getTypeManager() const;
};

  }
}
