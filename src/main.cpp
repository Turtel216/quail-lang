#include "../include/ast.hpp"
#include "parser.hpp"
#include <memory>

void yy::parser::error(const std::string &msg) {
  std::cout << "An error occured: " << msg << std::endl;
}

extern std::vector<std::unique_ptr<Definition>> program;

int main() {
  yy::parser parser;
  parser.parse();
  std::cout << program.size() << std::endl;
}
