#pragma once

#include "binop.h"
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

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
  int count;

public:
  Pop(int _count) noexcept : count(_count) {}

  void print(int indent, std::ostream &to) const override;
  int getCount() const noexcept { return this->count; }
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
  int size;

public:
  Pack(int _tag, int _size) noexcept : tag(_tag), size(_size) {}

  void print(int indent, std::ostream &to) const override;
  int getTag() const noexcept { return this->tag; }
  int getSize() const noexcept { return this->size; }
};

class Split : public Instruction {
public:
  void print(int indent, std::ostream &to) const override;
};

// TODO: Determine private variable accesors and mutators
class Jump : public Instruction {
private:
  std::vector<std::vector<std::unique_ptr<Instruction>>> branches;
  std::map<int, int> tagMappings;

public:
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
  int amount;

public:
  Alloc(int _amount) noexcept : amount(_amount) {}

  void print(int indent, std::ostream &to) const override;
  int getAmount() const noexcept { return this->amount; }
};

} // namespace ir
