
#pragma once

#include "binop.hpp"
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

  virtual void print(int indent, std::ostream &to) const = 0;
};

class PushInt : public Instruction {
private:
  int value;

public:
  PushInt(int _value) noexcept : value(_value) {}

  void print(int indent, std::ostream &to) const override;
  int getValue() const noexcept { return this->value; }
};

class PushGlobal : public Instruction {
private:
  std::string name;

public:
  PushGlobal(std::string _name) noexcept : name(std::move(_name)) {}

  void print(int indent, std::ostream &to) const override;
  std::string getName() const noexcept { return this->name; }
};

class Push : public Instruction {
private:
  int offset;

public:
  Push(int _offset) noexcept : offset(_offset) {}

  void print(int indent, std::ostream &to) const override;
  int getOffset() const noexcept { return this->offset; }
};

class Pop : public Instruction {
private:
  std::size_t count;

public:
  Pop(std::size_t _count) noexcept : count(_count) {}

  void print(int indent, std::ostream &to) const override;
  std::size_t getCount() const noexcept { return this->count; }
};

// Apply a function at the top of the stack to a value after it.
class MkApp : public Instruction {
public:
  void print(int indent, std::ostream &to) const override;
};

class Update : public Instruction {
private:
  int offset;

public:
  Update(int _offset) noexcept : offset(_offset) {}

  void print(int indent, std::ostream &to) const override;
  int getOffset() const noexcept { return this->offset; }
};

class Pack : public Instruction {
private:
  int tag;
  std::size_t size;

public:
  Pack(int _tag, std::size_t _size) noexcept : tag(_tag), size(_size) {}

  void print(int indent, std::ostream &to) const override;
  int getTag() const noexcept { return this->tag; }
  std::size_t getSize() const noexcept { return this->size; }
};

class Split : public Instruction {
public:
  void print(int indent, std::ostream &to) const override;
};

// TODO: Determine private variable accesors and mutators
class Jump : public Instruction {
public:
  std::vector<std::vector<std::unique_ptr<Instruction>>> branches;
  std::map<int, int> tagMappings;

  void print(int indent, std::ostream &to) const override;
};

class Slide : public Instruction {
private:
  int offset;

public:
  Slide(int _offset) noexcept : offset(_offset) {}
  void print(int indent, std::ostream &to) const override;
};

class Binop : public Instruction {
private:
  binop op;

public:
  Binop(binop _op) noexcept : op(_op) {}

  void print(int indent, std::ostream &to) const override;
};

class Eval : public Instruction {
public:
  void print(int indent, std::ostream &to) const override;
};

class Alloc : public Instruction {
private:
  std::size_t amount;

public:
  Alloc(std::size_t _amount) noexcept : amount(_amount) {}

  void print(int indent, std::ostream &to) const override;
  std::size_t getAmount() const noexcept { return this->amount; }
};

class Unwind : public Instruction {
  void print(int indent, std::ostream &to) const override;
};
} // namespace ir
} // namespace ff
