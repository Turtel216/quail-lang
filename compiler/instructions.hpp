#pragma once

#include "binop.hpp"
#include "generator.hpp"
#include <cstddef>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace ff {
namespace ir {
class Instruction {

public:
  virtual ~Instruction() = default;

  virtual void generate(cg::CodeGenerator &, llvm::Function *) const = 0;
  virtual void print(int indent, std::ostream &to) const = 0;
};

class PushInt : public Instruction {
private:
  int value;

public:
  PushInt(int _value) noexcept : value(_value) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
  inline int getValue() const noexcept { return this->value; }
};

class PushGlobal : public Instruction {
private:
  std::string name;

public:
  PushGlobal(std::string _name) noexcept : name(std::move(_name)) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
  inline std::string getName() const noexcept { return this->name; }
};

class Push : public Instruction {
private:
  int offset;

public:
  Push(int _offset) noexcept : offset(_offset) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
  inline int getOffset() const noexcept { return this->offset; }
};

class Pop : public Instruction {
private:
  std::size_t count;

public:
  Pop(std::size_t _count) noexcept : count(_count) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
  inline std::size_t getCount() const noexcept { return this->count; }
};

// Apply a function at the top of the stack to a value after it.
class MkApp : public Instruction {
public:
  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
};

class Update : public Instruction {
private:
  int offset;

public:
  Update(int _offset) noexcept : offset(_offset) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
  inline int getOffset() const noexcept { return this->offset; }
};

class Pack : public Instruction {
private:
  int tag;
  std::size_t size;

public:
  Pack(int _tag, std::size_t _size) noexcept : tag(_tag), size(_size) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
  inline int getTag() const noexcept { return this->tag; }
  inline std::size_t getSize() const noexcept { return this->size; }
};

class Split : public Instruction {
public:
  int size;

  Split(int _size) noexcept : size(_size) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
};

// TODO: Determine private variable accesors and mutators
class Jump : public Instruction {

public:
  std::vector<std::vector<std::unique_ptr<Instruction>>> branches;
  std::map<int, int> tagMappings;

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
};

class Slide : public Instruction {
private:
  int offset;

public:
  Slide(int _offset) noexcept : offset(_offset) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
};

class Binop : public Instruction {
private:
  binop op;

public:
  Binop(binop _op) noexcept : op(_op) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
};

/* Compare the two numbers on top of the stack and leave the Bool the
 * comparison answers to in their place. The tags come from the Bool type
 * the prelude declares, which is only known once it has been typechecked. */
class Compare : public Instruction {
private:
  binop op;
  int trueTag;
  int falseTag;

public:
  Compare(binop _op, int _trueTag, int _falseTag) noexcept
      : op(_op), trueTag(_trueTag), falseTag(_falseTag) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
};

class Eval : public Instruction {
public:
  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
};

class Alloc : public Instruction {
private:
  std::size_t amount;

public:
  Alloc(std::size_t _amount) noexcept : amount(_amount) {}

  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
  inline std::size_t getAmount() const noexcept { return this->amount; }
};

class Unwind : public Instruction {
  void generate(cg::CodeGenerator &, llvm::Function *) const override;
  void print(int indent, std::ostream &to) const override;
};
} // namespace ir
} // namespace ff
