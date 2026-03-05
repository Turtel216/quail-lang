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
} // namespace cg
} // namespace ff
