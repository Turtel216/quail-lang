#include "../include/instructions.hpp"
#include <ostream>

namespace ir {

static void print_indent(int n, std::ostream &to) {
  while (n--)
    to << "  ";
}

void PushInt::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "PushInt(" << value << ")" << std::endl;
}

void PushGlobal::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "PushGlobal(" << this->name << ")" << std::endl;
}

void Push::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Push(" << offset << ")" << std::endl;
}

void Pop::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Pop(" << count << ")" << std::endl;
}

void MkApp::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "MkApp()" << std::endl;
}

void Update::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Update(" << offset << ")" << std::endl;
}

void Pack::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Pack(" << tag << ", " << size << ")" << std::endl;
}

void Split::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Split()" << std::endl;
}

void Jump::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Jump(" << std::endl;
  for (auto &instruction_set : branches) {
    for (auto &instruction : instruction_set) {
      instruction->print(indent + 2, to);
    }
    to << std::endl;
  }
  print_indent(indent, to);
  to << ")" << std::endl;
}

void Slide::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Slide(" << offset << ")" << std::endl;
}

void Binop::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "BinOp(" << opAction(op) << ")" << std::endl;
}

void Eval::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Eval()" << std::endl;
}

void Alloc::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Alloc(" << amount << ")" << std::endl;
}

void Unwind::print(int indent, std::ostream &to) const {
  print_indent(indent, to);
  to << "Unwind()" << std::endl;
}
} // namespace ir
