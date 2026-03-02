#include "../include/ast.hpp"
#include "../include/error.hpp"
#include "../include/types.hpp"
#include "instructions.hpp"
#include "parser.hpp"

void yy::parser::error(const std::string &msg) {
  std::cout << "An error occured: " << msg << std::endl;
}

extern std::vector<std::unique_ptr<Definition>> program;

void typecheck_program(const std::vector<std::unique_ptr<Definition>> &prog,
                       ff::sem::TypeManager &mgr, ff::sem::TypeContext &env) {
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

  for (auto &pair : env.names) {
    std::cout << pair.first << ": ";
    pair.second->print(mgr, std::cout);
    std::cout << std::endl;
  }

  for (auto &def : prog) {
    def->resolve(mgr);
  }
}

int main() {
  yy::parser parser;
  ff::sem::TypeManager mgr;
  ff::sem::TypeContext env;

  parser.parse();
  for (auto &definition : program) {
    DefinitionDefn *def = dynamic_cast<DefinitionDefn *>(definition.get());
    if (!def)
      continue;

    std::cout << def->name;
    for (auto &param : def->params)
      std::cout << " " << param;
    std::cout << ":" << std::endl;

    def->body->print(1, std::cout);
  }
  try {
    typecheck_program(program, mgr, env);
  } catch (ff::UnificationError &err) {
    std::cout << "failed to unify types: " << std::endl;
    std::cout << "  (1) \033[34m";
    err.left->print(mgr, std::cout);

    std::cout << "\033[0m" << std::endl;
    std::cout << "  (2) \033[32m";
    err.right->print(mgr, std::cout);

    std::cout << "\033[0m" << std::endl;
  } catch (ff::TypeError &err) {
    std::cout << "failed to type check program: " << err.description
              << std::endl;
  }
}
