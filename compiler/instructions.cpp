#include "instructions.hpp"
#include <ostream>

namespace ff {
namespace ir {

static void printIndent(int n, std::ostream &to) {
  while (n--)
    to << "  ";
}

void PushInt::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  generator.createPush(f, generator.createNum(f, generator.createI32(value)));
}

void PushInt::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "PushInt(" << value << ")" << std::endl;
}

void PushGlobal::generate(cg::CodeGenerator &generator,
                          llvm::Function *f) const {
  auto &global = generator.getCustomFunction(name);

  auto arity = generator.createI32(global.arity);
  generator.createPush(f, generator.createGlobal(f, global.function, arity));
}

void PushGlobal::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "PushGlobal(" << this->name << ")" << std::endl;
}

void Push::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  generator.createPush(
      f, generator.createPeek(f, generator.createSize(this->offset)));
}

void Push::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Push(" << offset << ")" << std::endl;
}

void Pop::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  generator.createPop(f, generator.createSize(this->count));
}

void Pop::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Pop(" << count << ")" << std::endl;
}

void MkApp::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  auto left = generator.createPop(f);
  auto right = generator.createPop(f);

  generator.createPush(f, generator.createApp(f, left, right));
}

void MkApp::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "MkApp()" << std::endl;
}

void Update::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  generator.createUpdate(f, generator.createSize(this->offset));
}

void Update::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Update(" << offset << ")" << std::endl;
}

void Pack::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  generator.createPack(f, generator.createSize(this->size),
                       generator.createI8(this->tag));
}

void Pack::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Pack(" << tag << ", " << size << ")" << std::endl;
}

void Split::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  generator.createSplit(f, generator.createSize(this->size));
}

void Split::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Split()" << std::endl;
}

void Jump::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  auto topNode = generator.createPeek(f, generator.createSize(0));
  auto tag = generator.unwrapDataTag(topNode);
  auto safetyBlock = generator.createBasicBlock("safety", f);
  auto switchOp =
      generator.getBuilder().CreateSwitch(tag, safetyBlock, tagMappings.size());

  std::vector<llvm::BasicBlock *> blocks;
  for (auto &branch : this->branches) {
    auto branchBlock = generator.createBasicBlock("branch", f);
    generator.getBuilder().SetInsertPoint(branchBlock);

    for (auto &instruction : branch) {
      instruction->generate(generator, f);
    }

    generator.getBuilder().CreateBr(safetyBlock);
    blocks.push_back(branchBlock);
  }

  for (auto &mapping : this->tagMappings) {
    switchOp->addCase(generator.createI8(mapping.first),
                      blocks[mapping.second]);
  }

  generator.getBuilder().SetInsertPoint(safetyBlock);
}

void Jump::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Jump(" << std::endl;
  for (auto &instructionSet : branches) {
    for (auto &instruction : instructionSet) {
      instruction->print(indent + 2, to);
    }
    to << std::endl;
  }
  printIndent(indent, to);
  to << ")" << std::endl;
}

void Slide::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  generator.createSlide(f, generator.createSize(this->offset));
}

void Slide::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Slide(" << offset << ")" << std::endl;
}

void Binop::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  auto leftInt = generator.unwrapNum(generator.createPop(f));
  auto rightInt = generator.unwrapNum(generator.createPop(f));

  llvm::Value *result;
  switch (op) {
  case PLUS:
    result = generator.getBuilder().CreateAdd(leftInt, rightInt);
    break;
  case MINUS:
    result = generator.getBuilder().CreateSub(leftInt, rightInt);
    break;
  case TIMES:
    result = generator.getBuilder().CreateMul(leftInt, rightInt);
    break;
  case DIVIDE:
    result = generator.getBuilder().CreateSDiv(leftInt, rightInt);
    break;
  }

  generator.createPush(f, generator.createNum(f, result));
}

void Binop::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "BinOp(" << opAction(op) << ")" << std::endl;
}

void Eval::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  generator.createUnwind(f);
}

void Eval::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Eval()" << std::endl;
}

void Alloc::generate(cg::CodeGenerator &generator, llvm::Function *f) const {
  generator.createAlloc(f, generator.createSize(this->amount));
}

void Alloc::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Alloc(" << amount << ")" << std::endl;
}

void Unwind::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "Unwind()" << std::endl;
}
} // namespace ir
} // namespace ff
