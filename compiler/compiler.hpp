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

        void addDefaultTypes();
        void addBinopType(binop op, std::shared_ptr<sem::Type> type);
        void addDefaultFunctionTypes();
        void parseFile(const std::string& path);
        void parse();
        void typecheck();
        void translate();
        void compileDefinition(DefinitionDefn &definition);
        void compile();
        void createLLVMBinop(binop op);
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
