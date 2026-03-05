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

  this->functions["stack_slide"] = Function::Create(
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
} // namespace cg
} // namespace ff
