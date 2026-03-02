#pragma once

#include "../include/binop.hpp"
#include "../include/context.hpp"
#include "../include/types.hpp"
#include "enviroment.hpp"
#include "instructions.hpp"
#include <memory>
#include <string>
#include <vector>

class Ast {
public:
  std::shared_ptr<ff::sem::Type> nodeType;

  virtual ~Ast() = default;

  void commonResolve(const ff::sem::TypeManager &mgr);
  virtual void resolve(const ff::sem::TypeManager &mgr) const = 0;

  std::shared_ptr<ff::sem::Type>
  commonTypecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeContext &context);
  virtual std::shared_ptr<ff::sem::Type>
  typecheck(ff::sem::TypeManager &mgr,
            const ff::sem::TypeContext &env) const = 0;

  virtual void
  generate(const std::shared_ptr<ff::ir::Enviroment> &env,
           std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const = 0;

  virtual void print(int indent, std::ostream &to) const = 0;
};

class Pattern {
public:
  virtual ~Pattern() = default;

  virtual void match(std::shared_ptr<ff::sem::Type> t,
                     ff::sem::TypeManager &mgr,
                     ff::sem::TypeContext &env) const = 0;

  virtual void print(std::ostream &to) const = 0;
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
  std::vector<std::string> types;
  int tag;

  Constructor(std::string _name, std::vector<std::string> _types)
      : name(std::move(_name)), types(std::move(_types)) {}
};

class Definition {
public:
  virtual ~Definition() = default;

  virtual void typeCheckFirst(ff::sem::TypeManager &mgr,
                              ff::sem::TypeContext &env) = 0;
  virtual void typeCheckSecond(ff::sem::TypeManager &mgr,
                               const ff::sem::TypeContext &env) const = 0;

  virtual void resolve(const ff::sem::TypeManager &mgr) = 0;

  virtual void generate() = 0;
};

class AstInt : public Ast {
public:
  int value;

  explicit AstInt(int v) : value(v) {}

  std::shared_ptr<ff::sem::Type>
  typecheck(ff::sem::TypeManager &mgr,
            const ff::sem::TypeContext &env) const override;

  void resolve(const ff::sem::TypeManager &mgr) const override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;
  void print(int indent, std::ostream &to) const override;
};

class AstLid : public Ast {
public:
  std::string id;

  explicit AstLid(std::string i) : id(std::move(i)) {}

  std::shared_ptr<ff::sem::Type>
  typecheck(ff::sem::TypeManager &mgr,
            const ff::sem::TypeContext &env) const override;

  void resolve(const ff::sem::TypeManager &mgr) const override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;
  void print(int indent, std::ostream &to) const override;
};

class AstUid : public Ast {
public:
  std::string id;

  explicit AstUid(std::string i) : id(std::move(i)) {}

  std::shared_ptr<ff::sem::Type>
  typecheck(ff::sem::TypeManager &mgr,
            const ff::sem::TypeContext &env) const override;

  void resolve(const ff::sem::TypeManager &mgr) const override;

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

  std::shared_ptr<ff::sem::Type>
  typecheck(ff::sem::TypeManager &mgr,
            const ff::sem::TypeContext &env) const override;

  void resolve(const ff::sem::TypeManager &mgr) const override;

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

  std::shared_ptr<ff::sem::Type>
  typecheck(ff::sem::TypeManager &mgr,
            const ff::sem::TypeContext &env) const override;

  void resolve(const ff::sem::TypeManager &mgr) const override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;
  void print(int indent, std::ostream &to) const override;
};

class AstCase : public Ast {
public:
  std::unique_ptr<Ast> of;
  std::vector<std::unique_ptr<Branch>> branches;

  AstCase(std::unique_ptr<Ast> _of,
          std::vector<std::unique_ptr<Branch>> _branches)
      : of(std::move(_of)), branches(std::move(_branches)) {}

  std::shared_ptr<ff::sem::Type>
  typecheck(ff::sem::TypeManager &mgr,
            const ff::sem::TypeContext &env) const override;

  void resolve(const ff::sem::TypeManager &mgr) const override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;
  void print(int indent, std::ostream &to) const override;
};

class PatternVar : public Pattern {
public:
  std::string var;

  PatternVar(std::string _var) : var(std::move(_var)) {}

  void match(std::shared_ptr<ff::sem::Type> t, ff::sem::TypeManager &mgr,
             ff::sem::TypeContext &env) const override;

  void print(std::ostream &to) const override;
};

class PatternConstr : public Pattern {
public:
  std::string constr;
  std::vector<std::string> params;

  PatternConstr(std::string c, std::vector<std::string> p)
      : constr(std::move(c)), params(std::move(p)) {}

  void match(std::shared_ptr<ff::sem::Type> t, ff::sem::TypeManager &mgr,
             ff::sem::TypeContext &env) const override;

  void print(std::ostream &to) const override;
};

class DefinitionDefn : public Definition {
public:
  std::string name;
  std::vector<std::string> params;
  std::unique_ptr<Ast> body;

  // Types
  std::shared_ptr<ff::sem::Type> returnType;
  std::vector<std::shared_ptr<ff::sem::Type>> paramTypes;

  std::vector<std::unique_ptr<ff::ir::Instruction>> instructions;

  DefinitionDefn(std::string _name, std::vector<std::string> _params,
                 std::unique_ptr<Ast> _body)
      : name(std::move(_name)), params(std::move(_params)),
        body(std::move(_body)) {}

  void typeCheckFirst(ff::sem::TypeManager &mgr,
                      ff::sem::TypeContext &env) override;
  void typeCheckSecond(ff::sem::TypeManager &mgr,
                       const ff::sem::TypeContext &env) const override;

  void resolve(const ff::sem::TypeManager &mgr) override;
  void generate() override;
};

class DefinitionData : public Definition {
public:
  std::string name;
  std::vector<std::unique_ptr<Constructor>> constructors;

  DefinitionData(std::string _name,
                 std::vector<std::unique_ptr<Constructor>> _constructors)
      : name(std::move(_name)), constructors(std::move(_constructors)) {}

  void typeCheckFirst(ff::sem::TypeManager &mgr,
                      ff::sem::TypeContext &env) override;
  void typeCheckSecond(ff::sem::TypeManager &mgr,
                       const ff::sem::TypeContext &env) const override;

  void resolve(const ff::sem::TypeManager &mgr) override;

  void generate() override;
};
