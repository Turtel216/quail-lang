#pragma once
#include <memory>
#include <string>
#include <vector>

class Ast {
public:
  virtual ~Ast() = default;
};

class Pattern {
public:
  virtual ~Pattern() = default;
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

  Constructor(std::string _name, std::vector<std::string> _types)
      : name(std::move(_name)), types(std::move(_types)) {}
};

class Definition {
public:
  virtual ~Definition() = default;
};

enum binop { PLUS, MINUS, TIMES, DIVIDE };

class AstInt : public Ast {
public:
  int value;

  explicit AstInt(int v) : value(v) {}
};

class AstLid : public Ast {
public:
  std::string id;

  explicit AstLid(std::string i) : id(std::move(i)) {}
};

class AstUid : public Ast {
public:
  std::string id;

  explicit AstUid(std::string i) : id(std::move(i)) {}
};

class AstBinop : public Ast {
public:
  binop op;
  std::unique_ptr<Ast> left;
  std::unique_ptr<Ast> right;

  AstBinop(binop _op, std::unique_ptr<Ast> lhs, std::unique_ptr<Ast> rhs)
      : op(_op), left(std::move(lhs)), right(std::move(rhs)) {}
};

class AstApp : public Ast {
public:
  std::unique_ptr<Ast> left;
  std::unique_ptr<Ast> right;

  AstApp(std::unique_ptr<Ast> lhs, std::unique_ptr<Ast> rhs)
      : left(std::move(lhs)), right(std::move(rhs)) {}
};

class AstCase : public Ast {
public:
  std::unique_ptr<Ast> of;
  std::vector<std::unique_ptr<Branch>> branches;

  AstCase(std::unique_ptr<Ast> _of,
          std::vector<std::unique_ptr<Branch>> _branches)
      : of(std::move(_of)), branches(std::move(_branches)) {}
};

class PatternVar : public Pattern {
public:
  std::string var;

  PatternVar(std::string _var) : var(std::move(_var)) {}
};

class PatternConstr : public Pattern {
public:
  std::string constr;
  std::vector<std::string> params;

  PatternConstr(std::string c, std::vector<std::string> p)
      : constr(std::move(c)), params(std::move(p)) {}
};

class DefinitionDefn : public Definition {
public:
  std::string name;
  std::vector<std::string> params;
  std::unique_ptr<Ast> body;

  DefinitionDefn(std::string _name, std::vector<std::string> _params,
                 std::unique_ptr<Ast> _body)
      : name(std::move(_name)), params(std::move(_params)),
        body(std::move(_body)) {}
};

class DefinitionData : public Definition {
public:
  std::string name;
  std::vector<std::unique_ptr<Constructor>> constructors;

  DefinitionData(std::string _name,
                 std::vector<std::unique_ptr<Constructor>> _constructors)
      : name(std::move(_name)), constructors(std::move(_constructors)) {}
};
