#pragma once

#include <cstdint>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <map>
#include <string>

namespace ff {
namespace cg {
class CodeGenerator {
public: // TODO decide on encapsulation
  struct CustomFunction {
    llvm::Function *function;
    std::int32_t arity;
  };

  llvm::LLVMContext ctx;
  llvm::IRBuilder<> builder;
  llvm::Module module;

  std::map<std::string, std::unique_ptr<CustomFunction>> customFunctions;
  std::map<std::string, llvm::Function *> functions;
  std::map<std::string, llvm::StructType> structTypes;

  llvm::StructType *stackType;
  llvm::PointerType *stackPointerType;
  llvm::PointerType *nodePtrType;
  llvm::IntegerType *tagType;
  llvm::FunctionType *functionType;

  CodeGenerator() : builder(ctx), module("ff", ctx) {
    this->createTypes();
    this->createFunctions();
  }

  void createTypes();
  void createFunctions();

  llvm::ConstantInt *createI8(std::int8_t);
  llvm::ConstantInt *createI32(std::int32_t);
  llvm::ConstantInt *createSize(std::size_t);

  llvm::Value *createPop(llvm::Function *);
  llvm::Value *createPeek(llvm::Function *, llvm::Value *);
  void createPush(llvm::Function *, llvm::Value *);
  void createPop(llvm::Function *, llvm::Value *);
  void createUpdate(llvm::Function *, llvm::Value *);
  void createPack(llvm::Function *, llvm::Value *, llvm::Value *);
  void createSplit(llvm::Function *, llvm::Value *);
  void createSlide(llvm::Function *, llvm::Value *);
  void createAlloc(llvm::Function *, llvm::Value *);
  llvm::Value *createEval(llvm::Value *);

  llvm::Value *unwrapNum(llvm::Value *);
  llvm::Value *createNum(llvm::Value *);

  llvm::Value *unwrapDataTag(llvm::Value *);

  llvm::Value *createGlobal(llvm::Value *, llvm::Value *);

  llvm::Value *createApp(llvm::Value *, llvm::Value *);

  llvm::Function *createCustomFunction(std::string name, int32_t arity);
};
} // namespace cg
} // namespace ff
