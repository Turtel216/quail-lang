#pragma once

#include "../include/binop.hpp"
#include "../include/context.hpp"
#include "../include/parsed_type.hpp"
#include "../include/types.hpp"
#include "enviroment.hpp"
#include "generator.hpp"
#include "instructions.hpp"
#include <llvm/IR/Function.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

class Ast {
public:
  std::shared_ptr<ff::sem::TypeContext> typeContext;

  virtual ~Ast() = default;

  virtual std::shared_ptr<ff::sem::Type>
  typecheck(ff::sem::TypeManager &mgr) = 0;

  virtual void findFree(ff::sem::TypeManager &mgr,
                        std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                        std::set<std::string> &into) = 0;

  virtual void
  generate(const std::shared_ptr<ff::ir::Enviroment> &env,
           std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const = 0;

  virtual void print(int indent, std::ostream &to) const = 0;
};

class Pattern {
public:
  virtual ~Pattern() = default;

  virtual void print(std::ostream &to) const = 0;
  virtual void
  insertBindings(ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const = 0;
  virtual void
  typecheck(std::shared_ptr<ff::sem::Type>, ff::sem::TypeManager &mgr,
            std::shared_ptr<ff::sem::TypeContext> &typeCtx) const = 0;
};

class Branch {
public:
  std::unique_ptr<Pattern> pattern;
  std::unique_ptr<Ast> expr;

  Branch(std::unique_ptr<Pattern> _pattern, std::unique_ptr<Ast> _expr)
      : pattern(std::move(_pattern)), expr(std::move(_expr)) {}
};

class Constructor {
public:
  std::string name;
  std::vector<std::unique_ptr<ff::sem::ParsedType>> types;
  int tag;

  Constructor(std::string _name,
              std::vector<std::unique_ptr<ff::sem::ParsedType>> _types)
      : name(std::move(_name)), types(std::move(_types)) {}
};

class AstInt : public Ast {
public:
  int value;

  explicit AstInt(int v) : value(v) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void print(int indent, std::ostream &to) const override;
};

class AstLid : public Ast {
public:
  std::string id;

  explicit AstLid(std::string i) : id(std::move(i)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void print(int indent, std::ostream &to) const override;
};

class AstUid : public Ast {
public:
  std::string id;

  explicit AstUid(std::string i) : id(std::move(i)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;
  void print(int indent, std::ostream &to) const override;
};

class AstBinop : public Ast {
public:
  binop op;
  std::unique_ptr<Ast> left;
  std::unique_ptr<Ast> right;

  AstBinop(binop _op, std::unique_ptr<Ast> lhs, std::unique_ptr<Ast> rhs)
      : op(_op), left(std::move(lhs)), right(std::move(rhs)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void print(int indent, std::ostream &to) const override;
};

class AstApp : public Ast {
public:
  std::unique_ptr<Ast> left;
  std::unique_ptr<Ast> right;

  AstApp(std::unique_ptr<Ast> lhs, std::unique_ptr<Ast> rhs)
      : left(std::move(lhs)), right(std::move(rhs)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;
  void print(int indent, std::ostream &to) const override;
};

class AstCase : public Ast {
public:
  std::unique_ptr<Ast> of;
  std::vector<std::unique_ptr<Branch>> branches;
  std::shared_ptr<ff::sem::Type> inputType;

  AstCase(std::unique_ptr<Ast> _of,
          std::vector<std::unique_ptr<Branch>> _branches)
      : of(std::move(_of)), branches(std::move(_branches)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void print(int indent, std::ostream &to) const override;
};

class PatternVar : public Pattern {
public:
  std::string var;

  PatternVar(std::string _var) : var(std::move(_var)) {}

  void
  insertBindings(ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const override;

  void typecheck(std::shared_ptr<ff::sem::Type>, ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const override;

  void print(std::ostream &to) const override;
};

class PatternConstr : public Pattern {
public:
  std::string constr;
  std::vector<std::string> params;

  PatternConstr(std::string c, std::vector<std::string> p)
      : constr(std::move(c)), params(std::move(p)) {}

  void
  insertBindings(ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const override;

  void typecheck(std::shared_ptr<ff::sem::Type>, ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const override;

  void print(std::ostream &to) const override;
};

class DefinitionDefn {
public: // TODO: Fix encapsulation
  std::string name;
  std::vector<std::string> params;
  std::unique_ptr<Ast> body;

  std::shared_ptr<ff::sem::TypeContext> typeContext;
  std::shared_ptr<ff::sem::TypeContext> varContext;
  std::set<std::string> freeVariables;
  std::shared_ptr<ff::sem::Type> fullType;
  std::shared_ptr<ff::sem::Type> returnType;

  std::vector<std::unique_ptr<ff::ir::Instruction>> instructions;

  llvm::Function *generatedFunction;

  DefinitionDefn(std::string n, std::vector<std::string> p,
                 std::unique_ptr<Ast> b)
      : name(std::move(n)), params(std::move(p)), body(std::move(b)) {}

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx);
  void insertTypes(ff::sem::TypeManager &mgr);
  void typecheck(ff::sem::TypeManager &mgr);
  void compile();
  void declareLLVM(ff::cg::CodeGenerator &generator);
  void generateLLVM(ff::cg::CodeGenerator &generator);
};

class DefinitionData {
public:
  std::string name;
  std::vector<std::unique_ptr<Constructor>> constructors;
  std::vector<std::string> vars;

  std::shared_ptr<ff::sem::TypeContext> typeContext;

  DefinitionData(std::string n, std::vector<std::string> _vars,
                 std::vector<std::unique_ptr<Constructor>> cs)
      : name(std::move(n)), constructors(std::move(cs)),
        vars(std::move(_vars)) {}

  void insertTypes(std::shared_ptr<ff::sem::TypeContext> &typeCtx);
  void insertConstructors() const;
  void generateLLVM(ff::cg::CodeGenerator &generator);
};
