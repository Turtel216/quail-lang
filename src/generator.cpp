#include "../include/generator.hpp"
#include <cstdio>
#include <llvm/IR/DerivedTypes.h>

namespace ff {
namespace cg {
void CodeGenerator::createTypes() {
  this->stackType = llvm::StructType::create(this->ctx, "stack");
  this->gmachineType = llvm::StructType::create(this->ctx, "gmachine");
  this->stackPointerType = llvm::PointerType::getUnqual(this->stackType);
  this->gmachinePtrType = llvm::PointerType::getUnqual(this->gmachineType);
  this->tagType = llvm::IntegerType::getInt8Ty(this->ctx);

  this->structTypes["node_base"] =
      llvm::StructType::create(this->ctx, "node_base");
  this->structTypes["node_app"] =
      llvm::StructType::create(this->ctx, "node_app");

  this->structTypes["node_global"] =
      llvm::StructType::create(this->ctx, "node_global");
  this->structTypes["node_ind"] =
      llvm::StructType::create(this->ctx, "node_ind");
  this->structTypes["node_data"] =
      llvm::StructType::create(this->ctx, "node_data");

  this->nodePtrType = llvm::PointerType::getUnqual(structTypes.at("node_base"));
  this->functionType = llvm::FunctionType::get(llvm::Type::getVoidTy(this->ctx),
                                               {this->gmachinePtrType}, false);
  this->gmachineType->setBody(this->stackPointerType, this->nodePtrType);

  this->structTypes.at("node_base")
      ->setBody({llvm::IntegerType::getInt64Ty(this->ctx), this->nodePtrType});

  this->structTypes.at("node_app")
      ->setBody({this->structTypes.at("node_base"), this->nodePtrType,
                 this->nodePtrType});



  this->structTypes.at("node_global")
      ->setBody({this->structTypes.at("node_base"),
                 llvm::FunctionType::get(llvm::Type::getVoidTy(ctx),
                                         {this->stackPointerType}, false)});
  this->structTypes.at("node_ind")
      ->setBody({this->structTypes.at("node_base"), this->nodePtrType});

  /* node_data: the data constructor tag is now in the header's spare bits,
   * so the struct is just {node_base, node_base**}. */
  this->structTypes.at("node_data")
      ->setBody({this->structTypes.at("node_base"),
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

  this->functions["gmachine_slide"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->gmachinePtrType, sizetType},
                              false),
      llvm::Function::LinkageTypes::ExternalLinkage, "gmachine_slide",
      &this->module);

  this->functions["gmachine_update"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->gmachinePtrType, sizetType},
                              false),
      llvm::Function::LinkageTypes::ExternalLinkage, "gmachine_update",
      &module);

  this->functions["gmachine_alloc"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->gmachinePtrType, sizetType},
                              false),
      llvm::Function::LinkageTypes::ExternalLinkage, "gmachine_alloc",
      &this->module);

  this->functions["gmachine_pack"] = llvm::Function::Create(
      llvm::FunctionType::get(
          voidType, {this->gmachinePtrType, sizetType, tagType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "gmachine_pack",
      &this->module);

  functions["gmachine_split"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->gmachinePtrType, sizetType},
                              false),
      llvm::Function::LinkageTypes::ExternalLinkage, "gmachine_split",
      &this->module);

  auto int32Type = llvm::IntegerType::getInt32Ty(ctx);

  /* alloc_app now takes (gmachine*, node_base*, node_base*) -> node_base*.
   * The gmachine pointer gives access to the minor heap. */
  functions["alloc_app"] = llvm::Function::Create(
      llvm::FunctionType::get(
          this->nodePtrType,
          {this->gmachinePtrType, this->nodePtrType, this->nodePtrType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "alloc_app",
      &this->module);

  /* alloc_global: (gmachine*, function, i32) -> node_base* */
  this->functions["alloc_global"] = llvm::Function::Create(
      llvm::FunctionType::get(
          this->nodePtrType,
          {this->gmachinePtrType, this->functionType, int32Type}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "alloc_global",
      &this->module);

  /* alloc_ind: (gmachine*, node_base*) -> node_base* */
  this->functions["alloc_ind"] = llvm::Function::Create(
      llvm::FunctionType::get(
          this->nodePtrType,
          {this->gmachinePtrType, this->nodePtrType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "alloc_ind",
      &this->module);

  this->functions["unwind"] = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {this->gmachinePtrType}, false),
      llvm::Function::LinkageTypes::ExternalLinkage, "unwind", &this->module);
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
  return this->builder.CreateCall(pop,
                                  {unwrapGmachineStackPtr(f->arg_begin())});
}

llvm::Value *CodeGenerator::createPeek(llvm::Function *f, llvm::Value *off) {
  auto peek = this->functions.at("stack_peek");
  return this->builder.CreateCall(
      peek, {unwrapGmachineStackPtr(f->arg_begin()), off});
}

void CodeGenerator::createPush(llvm::Function *f, llvm::Value *v) {
  auto push = this->functions.at("stack_push");
  this->builder.CreateCall(push, {unwrapGmachineStackPtr(f->arg_begin()), v});
}

void CodeGenerator::createPop(llvm::Function *f, llvm::Value *off) {
  auto popn = this->functions.at("stack_popn");
  builder.CreateCall(popn, {unwrapGmachineStackPtr(f->arg_begin()), off});
}

void CodeGenerator::createUpdate(llvm::Function *f, llvm::Value *off) {
  auto update = this->functions.at("gmachine_update");
  this->builder.CreateCall(update,
                           {unwrapGmachineStackPtr(f->arg_begin()), off});
}

void CodeGenerator::createPack(llvm::Function *f, llvm::Value *c,
                               llvm::Value *t) {
  auto pack = this->functions.at("gmachine_pack");
  this->builder.CreateCall(pack, {f->arg_begin(), c, t});
}

void CodeGenerator::createSplit(llvm::Function *f, llvm::Value *c) {
  auto split = this->functions.at("gmachine_split");
  this->builder.CreateCall(split, {f->arg_begin(), c});
}

void CodeGenerator::createSlide(llvm::Function *f, llvm::Value *off) {
  auto slide = this->functions.at("gmachine_slide");
  this->builder.CreateCall(slide, {f->arg_begin(), off});
}

void CodeGenerator::createAlloc(llvm::Function *f, llvm::Value *n) {
  auto alloc = this->functions.at("gmachine_alloc");
  this->builder.CreateCall(alloc, {f->arg_begin(), n});
}

llvm::Value *CodeGenerator::createEval(llvm::Value *e) {
  auto eval = this->functions.at("eval");
  return this->builder.CreateCall(eval, {e});
}

llvm::Value *CodeGenerator::unwrapNum(llvm::Value *v) {
  /* Inline untagging: ptrtoint → ashr 1 → trunc to i32.
   * The tagged value has LSB=1 and the integer in the upper bits.
   * Arithmetic right-shift preserves the sign. */
  auto i64Type = llvm::IntegerType::getInt64Ty(this->ctx);
  auto i32Type = llvm::IntegerType::getInt32Ty(this->ctx);

  auto raw = this->builder.CreatePtrToInt(v, i64Type, "untag.raw");
  auto shifted = this->builder.CreateAShr(
      raw, llvm::ConstantInt::get(i64Type, 1), "untag.shifted");
  return this->builder.CreateTrunc(shifted, i32Type, "untag.i32");
}

llvm::Value *CodeGenerator::createNum(llvm::Function *f, llvm::Value *v) {
  /* Inline tagging: sext i32 → i64, shl 1, or 1, inttoptr.
   * Produces a tagged pointer with LSB=1 — zero allocation. */
  (void)f; /* No longer needs the function for gmachine_track */
  auto i64Type = llvm::IntegerType::getInt64Ty(this->ctx);

  auto ext = this->builder.CreateSExt(v, i64Type, "tag.ext");
  auto shifted = this->builder.CreateShl(
      ext, llvm::ConstantInt::get(i64Type, 1), "tag.shifted");
  auto tagged = this->builder.CreateOr(
      shifted, llvm::ConstantInt::get(i64Type, 1), "tag.tagged");
  return this->builder.CreateIntToPtr(tagged, this->nodePtrType, "tag.ptr");
}

llvm::Value *CodeGenerator::unwrapDataTag(llvm::Value *v) {
  /* Extract the data constructor tag from the header's bits [11:4].
   * The header is field 0 of node_base, which is field 0 of any node struct.
   * Since v is a node_base*, GEP {0, 0} reaches the uint64_t header. */
  auto baseType = this->structTypes.at("node_base");
  auto i64Type = llvm::IntegerType::getInt64Ty(this->ctx);
  auto i8Type = llvm::IntegerType::getInt8Ty(this->ctx);
  auto offset0 = this->createI32(0);

  auto headerPtr =
      this->builder.CreateGEP(baseType, v, {offset0, offset0}, "hdr.ptr");
  auto header = this->builder.CreateLoad(i64Type, headerPtr, "hdr.val");
  auto shifted = this->builder.CreateLShr(
      header, llvm::ConstantInt::get(i64Type, 4), "dtag.shifted");
  auto masked = this->builder.CreateAnd(
      shifted, llvm::ConstantInt::get(i64Type, 0xFF), "dtag.masked");
  return this->builder.CreateTrunc(masked, i8Type, "dtag.i8");
}

llvm::Value *CodeGenerator::createGlobal(llvm::Function *f, llvm::Value *gf,
                                          llvm::Value *a) {
  auto allocGlobal = this->functions.at("alloc_global");
  /* Pass the gmachine pointer -- alloc_global bump-allocates from the
   * minor heap and handles GC internally, no separate track call. */
  return builder.CreateCall(allocGlobal, {f->arg_begin(), gf, a});
}

llvm::Value *CodeGenerator::createApp(llvm::Function *f, llvm::Value *l,
                                       llvm::Value *r) {
  auto allocApp = this->functions.at("alloc_app");
  /* Pass the gmachine pointer -- alloc_app bump-allocates from the
   * minor heap and handles GC internally, no separate track call. */
  return builder.CreateCall(allocApp, {f->arg_begin(), l, r});
}

llvm::Function *CodeGenerator::createCustomFunction(std::string name,
                                                    std::int32_t arity) {
  auto voidType = llvm::Type::getVoidTy(this->ctx);
  auto newFunction = llvm::Function::Create(
      this->functionType, llvm::Function::LinkageTypes::ExternalLinkage,
      "f_" + name, &this->module);

  auto startBlock = llvm::BasicBlock::Create(ctx, "entry", newFunction);

  auto newCustome = std::unique_ptr<CustomFunction>(new CustomFunction());

  newCustome->arity = arity;
  newCustome->function = newFunction;
  this->customFunctions["f_" + name] = std::move(newCustome);

  return newFunction;
}

void CodeGenerator::createUnwind(llvm::Function *f) {
  auto unwind = this->functions.at("unwind");
  this->builder.CreateCall(unwind, {f->args().begin()});
}



llvm::Value *CodeGenerator::unwrapGmachineStackPtr(llvm::Value *g) {
  auto offset0 = this->createI32(0);
  return builder.CreateGEP(this->gmachineType, g, {offset0, offset0});
}
} // namespace cg
} // namespace ff
