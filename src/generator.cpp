#include "../include/generator.hpp"
#include <llvm/IR/DerivedTypes.h>

namespace ff {
namespace cg {
void CodeGenerator::createTypes() {
  this->stackType = llvm::StructType::create(this->ctx, "stack");
  this->stackPointerType = llvm::PointerType::getUnqual(this->stackType);
  this->tagType = llvm::IntegerType::getInt8Ty(this->ctx);

  this->structTypes["node_base"] =
      llvm::StructType::create(this->ctx, "node_base");
  this->structTypes["node_app"] =
      llvm::StructType::create(this->ctx, "node_app");
  this->structTypes["node_num"] =
      llvm::StructType::create(this->ctx, "node_num");
  this->structTypes["node_global"] =
      llvm::StructType::create(this->ctx, "node_global");
  this->structTypes["node_ind"] =
      llvm::StructType::create(this->ctx, "node_ind");
  this->structTypes["node_data"] =
      llvm::StructType::create(this->ctx, "node_data");

  this->nodePtrType = llvm::PointerType::getUnqual(structTypes.at("node_base"));
  this->functionType = llvm::FunctionType::get(llvm::Type::getVoidTy(this->ctx),
                                               {this->stackPointerType}, false);

  this->structTypes.at("node_base")
      ->setBody(llvm::IntegerType::getInt32Ty(this->ctx));

  this->structTypes.at("node_app")
      ->setBody({this->structTypes.at("node_base"), this->nodePtrType,
                 this->nodePtrType});
  this->structTypes.at("node_num")
      ->setBody(this->structTypes.at("node_base"),
                llvm::IntegerType::getInt32Ty(ctx));
  this->structTypes.at("node_global")
      ->setBody(this->structTypes.at("node_base"),
                llvm::FunctionType::get(llvm::Type::getVoidTy(ctx),
                                        {this->stackPointerType}, false));
  this->structTypes.at("node_ind")
      ->setBody(this->structTypes.at("node_base"), this->nodePtrType);
  this->structTypes.at("node_data")
      ->setBody({this->structTypes.at("node_base"),
                 llvm::IntegerType::getInt8Ty(this->ctx),
                 llvm::PointerType::getUnqual(this->nodePtrType)});
}

void CodeGenerator::createFunctions() {
  auto voidType = llvm::Type::getVoidTy(this->ctx);
  auto sizetType = llvm::IntegerType::get(this->ctx, sizeof(size_t) * 8);

  this->functions["stack_init"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->stackPointerType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_init",
      &this->module);

  this->functions["stack_free"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->stackPointerType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_free",
      &this->module);

  this->functions["stack_push"] = llvm::Function::Create(
      llvm::FunctionType::get(
          voidType, {this->stackPointerType, this->nodePtrType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_push",
      &this->module);

  this->functions["stack_pop"] = llvm::Function::Create(
      llvm::FunctionType::get(this->nodePtrType, {this->stackPointerType},
                              false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_pop",
      &this->module);

  this->functions["stack_peek"] = llvm::Function::Create(
      llvm::FunctionType::get(this->nodePtrType,
                              {this->stackPointerType, sizetType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_peek",
      &this->module);

  this->functions["stack_popn"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->stackPointerType, sizetType},
                              false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_popn",
      &this->module);

  this->functions["stack_slide"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->stackPointerType, sizetType},
                              false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_slide",
      &this->module);

  this->functions["stack_update"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->stackPointerType, sizetType},
                              false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_update",
      &this->module);

  this->functions["stack_alloc"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->stackPointerType, sizetType},
                              false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_alloc",
      &this->module);

  this->functions["stack_pack"] = llvm::Function::Create(
      llvm::FunctionType::get(
          voidType, {this->stackPointerType, sizetType, this->tagType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_pack",
      &this->module);

  this->functions["stack_split"] = llvm::Function::Create(
      llvm::FunctionType::get(this->nodePtrType,
                              {this->stackPointerType, sizetType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "stack_split",
      &this->module);

  auto int32Type = llvm::IntegerType::getInt32Ty(ctx);
  functions["alloc_app"] = llvm::Function::Create(
      llvm::FunctionType::get(this->nodePtrType,
                              {this->nodePtrType, this->nodePtrType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "alloc_app",
      &this->module);

  this->functions["alloc_num"] = llvm::Function::Create(
      llvm::FunctionType::get(this->nodePtrType, {int32Type}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "alloc_num", &module);

  this->functions["alloc_global"] = llvm::Function::Create(
      llvm::FunctionType::get(this->nodePtrType,
                              {this->functionType, int32Type}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "alloc_global",
      &this->module);

  this->functions["alloc_ind"] = llvm::Function::Create(
      llvm::FunctionType::get(this->nodePtrType, {this->nodePtrType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "alloc_ind",
      &this->module);

  this->functions["eval"] = llvm::Function::Create(
      llvm::FunctionType::get(this->nodePtrType, {this->nodePtrType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "eval", &this->module);
}

llvm::ConstantInt *CodeGenerator::createI8(std::int8_t i) {
  return llvm::ConstantInt::get(ctx, llvm::APInt(8, i));
}

llvm::ConstantInt *CodeGenerator::createI32(std::int32_t i) {
  return llvm::ConstantInt::get(ctx, llvm::APInt(32, i));
}

llvm::ConstantInt *CodeGenerator::createSize(std::size_t i) {
  return llvm::ConstantInt::get(ctx, llvm::APInt(sizeof(size_t) * 8, i));
}

llvm::Value *CodeGenerator::createPop(llvm::Function *f) {
  auto pop = this->functions.at("stack_pop");
  return this->builder.CreateCall(pop, {f->arg_begin()});
}

llvm::Value *CodeGenerator::createPeek(llvm::Function *f, llvm::Value *off) {
  auto peek = this->functions.at("stack_peek");
  return this->builder.CreateCall(peek, {f->arg_begin(), off});
}

void CodeGenerator::createPush(llvm::Function *f, llvm::Value *v) {
  auto push = this->functions.at("stack_push");
  this->builder.CreateCall(push, {f->arg_begin(), v});
}

void CodeGenerator::createPop(llvm::Function *f, llvm::Value *off) {
  auto popn = this->functions.at("stack_popn");
  builder.CreateCall(popn, {f->arg_begin(), off});
}

void CodeGenerator::createUpdate(llvm::Function *f, llvm::Value *off) {
  auto update = this->functions.at("stack_update");
  this->builder.CreateCall(update, {f->arg_begin(), off});
}

void CodeGenerator::createPack(llvm::Function *f, llvm::Value *c,
                               llvm::Value *t) {
  auto pack = this->functions.at("stack_pack");
  this->builder.CreateCall(pack, {f->arg_begin(), c, t});
}

void CodeGenerator::createSplit(llvm::Function *f, llvm::Value *c) {
  auto split = this->functions.at("stack_split");
  this->builder.CreateCall(split, {f->arg_begin(), c});
}

void CodeGenerator::createSlide(llvm::Function *f, llvm::Value *off) {
  auto slide = this->functions.at("stack_slide");
  this->builder.CreateCall(slide, {f->arg_begin(), off});
}

void CodeGenerator::createAlloc(llvm::Function *f, llvm::Value *n) {
  auto alloc = this->functions.at("stack_alloc");
  this->builder.CreateCall(alloc, {f->arg_begin(), n});
}

llvm::Value *CodeGenerator::createEval(llvm::Value *e) {
  auto eval = this->functions.at("eval");
  return this->builder.CreateCall(eval, {e});
}

llvm::Value *CodeGenerator::unwrapNum(llvm::Value *v) {
  auto structType = this->structTypes.at("node_num");
  auto numPtr = llvm::PointerType::getUnqual(structType);
  auto cast = this->builder.CreatePointerCast(v, numPtr);
  auto offset_0 = this->createI32(0);
  auto offset_1 = this->createI32(1);
  auto intPtr = this->builder.CreateGEP(structType, cast, {offset_0, offset_1});
  return this->builder.CreateLoad(structType, intPtr);
}

llvm::Value *CodeGenerator::createNum(llvm::Value *v) {
  auto allocNum = this->functions.at("alloc_num");
  return this->builder.CreateCall(allocNum, {v});
}

llvm::Value *CodeGenerator::unwrapDataTag(llvm::Value *v) {
  auto structType = this->structTypes.at("node_data");
  auto dataPtr = llvm::PointerType::getUnqual(structType);
  auto cast = this->builder.CreatePointerCast(v, dataPtr);
  auto offset_0 = this->createI32(0);
  auto offset_1 = this->createI32(1);
  auto tagPtr = this->builder.CreateGEP(structType, cast, {offset_0, offset_1});
  return this->builder.CreateLoad(structType, tagPtr);
}

llvm::Value *CodeGenerator::createGlobal(llvm::Value *f, llvm::Value *a) {
  auto allocGlobal = this->functions.at("alloc_global");
  return this->builder.CreateCall(allocGlobal, {f, a});
}

llvm::Value *CodeGenerator::createApp(llvm::Value *l, llvm::Value *r) {
  auto allocApp = this->functions.at("alloc_app");
  return this->builder.CreateCall(allocApp, {l, r});
}

llvm::Function *CodeGenerator::createCustomFunction(std::string name,
                                                    std::int32_t arity) {
  auto voidType = llvm::Type::getVoidTy(ctx);
  auto functionType =
      llvm::FunctionType::get(voidType, {this->stackPointerType}, false);
  auto newFunction = llvm::Function::Create(
      functionType, llvm::Function::LinkageTypes::ExternalLinkage, "f_" + name,
      &this->module);
  auto startBlock = llvm::BasicBlock::Create(ctx, "entry", newFunction);

  auto newCustome = std::unique_ptr<CustomFunction>(new CustomFunction());

  newCustome->arity = arity;
  newCustome->function = newFunction;
  this->customFunctions["f_" + name] = std::move(newCustome);

  return newFunction;
}

} // namespace cg
} // namespace ff
