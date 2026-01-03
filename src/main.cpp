#include "../include/ast.hpp"
#include "../include/types.hpp"
#include "parser.hpp"

void yy::parser::error(const std::string &msg) {
  std::cout << "An error occured: " << msg << std::endl;
}

extern std::vector<std::unique_ptr<Definition>> program;

void typecheck_program(const std::vector<std::unique_ptr<Definition>> &prog) {
  ff::sem::TypeManager mgr;
  ff::sem::TypeEnv env;

  std::shared_ptr<ff::sem::Type> int_type =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase("Int"));
  std::shared_ptr<ff::sem::Type> binop_type =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(
          int_type, std::shared_ptr<ff::sem::Type>(
                        new ff::sem::TypeArr(int_type, int_type))));

  env.bind("+", binop_type);
  env.bind("-", binop_type);
  env.bind("*", binop_type);
  env.bind("/", binop_type);

  for (auto &def : prog) {
    def->typeCheckFirst(mgr, env);
  }

  for (auto &def : prog) {
    def->typeCheckSecond(mgr, env);
  }
}

int main() {
  yy::parser parser;
  parser.parse();
  typecheck_program(program);
  std::cout << program.size() << std::endl;
}
