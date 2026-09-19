#pragma once

#include <cstdint>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <map>
#include <string>
#include <vector>

namespace ff {
namespace cg {
class CodeGenerator {
private:
  struct CustomFunction {
    llvm::Function *function;
    std::int32_t arity;
    /* Where the one node standing for this global lives, for a global that
     * takes no arguments and so is a value rather than a function. Null for
     * everything else, which is allocated afresh at every push. */
    llvm::GlobalVariable *cafSlot = nullptr;
  };

  std::map<std::string, std::unique_ptr<CustomFunction>> customFunctions;
  /* The globals given a slot, in the order they were given one, which is the
   * order the initializer fills them in. */
  std::vector<std::string> cafOrder;
  std::map<std::string, llvm::Function *> functions;
  std::map<std::string, llvm::StructType *> structTypes;

  llvm::LLVMContext ctx;
  llvm::IRBuilder<> builder;
  llvm::Module module;

  llvm::StructType *stackType;
  llvm::StructType *gmachineType;
  llvm::PointerType *stackPointerType;
  llvm::PointerType *gmachinePtrType;
  llvm::PointerType *nodePtrType;
  llvm::IntegerType *tagType;
  llvm::FunctionType *functionType;

public:
  CodeGenerator() : builder(ctx), module("quail", ctx) {
    this->createTypes();
    this->createFunctions();
  }

  void createTypes();
  void createFunctions();

  llvm::IRBuilder<> &getBuilder() noexcept;
  llvm::Module &getModule() noexcept;
  llvm::BasicBlock *createBasicBlock(const std::string &name,
                                     llvm::Function *f) noexcept;
  CustomFunction &getCustomFunction(const std::string &name);

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
  void createUnwind(llvm::Function *);
  llvm::Value *createEval(llvm::Value *);

  llvm::Value *unwrapNum(llvm::Value *);
  llvm::Value *createNum(llvm::Function *, llvm::Value *);

  llvm::Value *unwrapDataTag(llvm::Value *);
  llvm::Value *unwrapGmachineStackPtr(llvm::Value *g);
  llvm::Value *createGlobal(llvm::Function *, llvm::Value *, llvm::Value *);

  llvm::Value *createApp(llvm::Function *, llvm::Value *, llvm::Value *);

  llvm::Function *createCustomFunction(std::string name, int32_t arity);

  /* Give `name` one node of its own, so that every push of it is the same
   * node and forcing it once is forcing it for good. Only a global that
   * takes no arguments may have one: it is the value that is shared, and
   * one that takes arguments has no value until it has them. */
  void markAsCaf(const std::string &name);
  llvm::Value *createCafLoad(llvm::GlobalVariable *slot);

  /* Fill in and register every slot. Called once, before the program runs. */
  void createCafInitializer();
};
} // namespace cg
} // namespace ff
