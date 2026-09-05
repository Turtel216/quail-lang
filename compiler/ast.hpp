#pragma once

#include "binop.hpp"
#include "context.hpp"
#include "mangler.hpp"
#include "parsed_type.hpp"
#include "types.hpp"
#include "enviroment.hpp"
#include "generator.hpp"
#include "graph_function.hpp"
#include "instructions.hpp"
#include <llvm/IR/Function.h>
#include <location.hh>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class GlobalScope;

class Ast {
public:
  std::shared_ptr<ff::sem::TypeContext> typeContext;
  /* Where this subexpression came from, so an error about it can quote the
   * source instead of just describing it. */
  yy::location loc;

  Ast(yy::location l) : loc(std::move(l)) {}

  virtual ~Ast() = default;

  virtual std::shared_ptr<ff::sem::Type>
  typecheck(ff::sem::TypeManager &mgr) = 0;

  /* Collect every name this subtree refers to but does not itself bind.
   * Each construct that introduces names removes its own before passing the
   * set on, so what arrives at a definition is exactly what it must either
   * find globally or capture. */
  virtual void findFree(ff::sem::TypeManager &mgr,
                        std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                        std::set<std::string> &into) = 0;

  /* Lift the nested definitions in this subtree into `scope`, leaving behind
   * references to the global functions they became. */
  virtual void translate(GlobalScope &scope) = 0;

  virtual void
  generate(const std::shared_ptr<ff::ir::Enviroment> &env,
           std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const = 0;

  virtual void print(int indent, std::ostream &to) const = 0;
};

class Pattern {
public:
  yy::location loc;

  Pattern(yy::location l) : loc(std::move(l)) {}

  virtual ~Pattern() = default;

  virtual void print(std::ostream &to) const = 0;
  virtual void
  insertBindings(ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const = 0;
  virtual void eraseBindings(std::set<std::string> &from) const = 0;
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

  explicit AstInt(int v, yy::location lc = yy::location())
      : Ast(std::move(lc)), value(v) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void print(int indent, std::ostream &to) const override;
};

class AstLid : public Ast {
public:
  std::string id;
  /* Set on the references left behind by lambda lifting, whose id is already
   * the symbol of a global rather than a name the surrounding scope binds. */
  bool lifted;

  explicit AstLid(std::string i, yy::location lc = yy::location())
      : Ast(std::move(lc)), id(std::move(i)), lifted(false) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void print(int indent, std::ostream &to) const override;
};

class AstUid : public Ast {
public:
  std::string id;

  explicit AstUid(std::string i, yy::location lc = yy::location())
      : Ast(std::move(lc)), id(std::move(i)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;
  void print(int indent, std::ostream &to) const override;
};

/* A list literal. It stands for the same chain of Cons applications ending
 * in Nil that writing them out by hand would build. */
class AstList : public Ast {
public:
  std::vector<std::unique_ptr<Ast>> items;

  explicit AstList(std::vector<std::unique_ptr<Ast>> i,
                   yy::location lc = yy::location())
      : Ast(std::move(lc)), items(std::move(i)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

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

  AstBinop(binop _op, std::unique_ptr<Ast> lhs, std::unique_ptr<Ast> rhs,
           yy::location lc = yy::location())
      : Ast(std::move(lc)), op(_op), left(std::move(lhs)),
        right(std::move(rhs)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void print(int indent, std::ostream &to) const override;
};

class AstApp : public Ast {
public:
  std::unique_ptr<Ast> left;
  std::unique_ptr<Ast> right;

  AstApp(std::unique_ptr<Ast> lhs, std::unique_ptr<Ast> rhs,
         yy::location lc = yy::location())
      : Ast(std::move(lc)), left(std::move(lhs)), right(std::move(rhs)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;
  void print(int indent, std::ostream &to) const override;
};

/* `value |> function` hands the value on the left to the function on the
 * right, building the same application as writing them the other way round
 * would. It stays a node of its own so that a mistake in a pipeline can be
 * reported in terms of the pipeline the program actually wrote. */
class AstPipe : public Ast {
public:
  std::unique_ptr<Ast> value;
  std::unique_ptr<Ast> function;

  AstPipe(std::unique_ptr<Ast> v, std::unique_ptr<Ast> f,
          yy::location lc = yy::location())
      : Ast(std::move(lc)), value(std::move(v)), function(std::move(f)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

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
          std::vector<std::unique_ptr<Branch>> _branches,
          yy::location lc = yy::location())
      : Ast(std::move(lc)), of(std::move(_of)), branches(std::move(_branches)) {
  }

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

  void print(int indent, std::ostream &to) const override;
};

/* `if cond { ... } else { ... }`. It is a case analysis of the two Bool
 * constructors written without naming them, and compiles to the same jump. */
class AstIf : public Ast {
public:
  std::unique_ptr<Ast> condition;
  std::unique_ptr<Ast> thenBranch;
  std::unique_ptr<Ast> elseBranch;

  /* The tags the two branches answer to, read off the Bool type once
   * typechecking has found it. */
  int trueTag = 0;
  int falseTag = 0;

  AstIf(std::unique_ptr<Ast> c, std::unique_ptr<Ast> t, std::unique_ptr<Ast> e,
        yy::location lc = yy::location())
      : Ast(std::move(lc)), condition(std::move(c)), thenBranch(std::move(t)),
        elseBranch(std::move(e)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void print(int indent, std::ostream &to) const override;
};

class PatternVar : public Pattern {
public:
  std::string var;

  PatternVar(std::string _var, yy::location lc = yy::location())
      : Pattern(std::move(lc)), var(std::move(_var)) {}

  void
  insertBindings(ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const override;

  void eraseBindings(std::set<std::string> &from) const override;

  void typecheck(std::shared_ptr<ff::sem::Type>, ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const override;

  void print(std::ostream &to) const override;
};

class PatternConstr : public Pattern {
public:
  std::string constr;
  std::vector<std::string> params;

  PatternConstr(std::string c, std::vector<std::string> p,
                yy::location lc = yy::location())
      : Pattern(std::move(lc)), constr(std::move(c)), params(std::move(p)) {}

  void
  insertBindings(ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const override;

  void eraseBindings(std::set<std::string> &from) const override;

  void typecheck(std::shared_ptr<ff::sem::Type>, ff::sem::TypeManager &mgr,
                 std::shared_ptr<ff::sem::TypeContext> &typeCtx) const override;

  void print(std::ostream &to) const override;
};

class DefinitionDefn {
public: // TODO: Fix encapsulation
  std::string name;
  std::vector<std::string> params;
  std::unique_ptr<Ast> body;

  /* A local definition is reachable only through a stack slot, so anything
   * nested inside it that mentions one must take it as an extra parameter. */
  ff::sem::Visibility visibility;
  std::string mangledName;
  std::set<std::string> capturedVariables;

  std::shared_ptr<ff::sem::TypeContext> typeContext;
  std::shared_ptr<ff::sem::TypeContext> varContext;
  std::set<std::string> freeVariables;
  std::shared_ptr<ff::sem::Type> fullType;
  std::shared_ptr<ff::sem::Type> returnType;

  std::vector<std::unique_ptr<ff::ir::Instruction>> instructions;

  llvm::Function *generatedFunction;

  yy::location loc;

  DefinitionDefn(std::string n, std::vector<std::string> p,
                 std::unique_ptr<Ast> b, yy::location lc = yy::location())
      : name(std::move(n)), params(std::move(p)), body(std::move(b)),
        visibility(ff::sem::Visibility::Global), mangledName(name),
        loc(std::move(lc)) {}

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx);
  void insertTypes(ff::sem::TypeManager &mgr);
  void typecheck(ff::sem::TypeManager &mgr);
  void translate(GlobalScope &scope);
  void compile();
  void declareLLVM(ff::cg::CodeGenerator &generator);
  void generateLLVM(ff::cg::CodeGenerator &generator);
};

/* Emit the supercombinator behind a data constructor: pack its arguments
 * into a data node and update the redex with it. Shared by declared data
 * types and by the built-in list. */
void generateConstructorLLVM(ff::cg::CodeGenerator &generator,
                             const std::string &name, int tag,
                             std::size_t arity);

class DefinitionData {
public:
  std::string name;
  std::vector<std::unique_ptr<Constructor>> constructors;
  std::vector<std::string> vars;

  std::shared_ptr<ff::sem::TypeContext> typeContext;

  yy::location loc;

  DefinitionData(std::string n, std::vector<std::string> _vars,
                 std::vector<std::unique_ptr<Constructor>> cs,
                 yy::location lc = yy::location())
      : name(std::move(n)), constructors(std::move(cs)), vars(std::move(_vars)),
        loc(std::move(lc)) {}

  void insertTypes(std::shared_ptr<ff::sem::TypeContext> &typeCtx);
  void insertConstructors() const;
  void generateLLVM(ff::cg::CodeGenerator &generator);
};

/* Definitions that share a scope and may refer to one another: the whole
 * program at the top level, or the bindings of a single let. */
class DefinitionGroup {
public:
  std::map<std::string, std::unique_ptr<DefinitionData>> defsData;
  std::map<std::string, std::unique_ptr<DefinitionDefn>> defsDefn;

  std::shared_ptr<ff::sem::TypeContext> typeContext;
  /* Mutually recursive members, in dependency order. */
  std::vector<std::unique_ptr<ff::sem::Group>> groups;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                ff::sem::Visibility visibility, std::set<std::string> &into);
  void typecheck(ff::sem::TypeManager &mgr);
  void translate(GlobalScope &scope);
};

/* Registry of the functions produced by lambda lifting. Entries are
 * non-owning: each lifted definition stays owned by the AST node it came
 * from, which outlives code generation. */
class GlobalScope {
private:
  std::vector<DefinitionDefn *> definitions;
  Mangler* mng;

public:
  GlobalScope(Mangler& m) : mng(&m) {}

  /* Claim `name` as a symbol, suffixing it if something already took it. */
  std::string mangle(const std::string &name);

  void add(DefinitionDefn &definition);

  inline const std::vector<DefinitionDefn *> &getDefinitions() const noexcept {
    return this->definitions;
  }
};

class AstLambda : public Ast {
public:
  std::vector<std::string> params;
  std::unique_ptr<Ast> body;

  std::shared_ptr<ff::sem::TypeContext> varContext;
  std::set<std::string> freeVariables;
  std::shared_ptr<ff::sem::Type> fullType;
  std::shared_ptr<ff::sem::Type> returnType;

  /* Both filled in by translate: the global function the body became, and
   * the partial application that stands in for this node afterwards. */
  std::unique_ptr<DefinitionDefn> lifted;
  std::unique_ptr<Ast> translated;

  AstLambda(std::vector<std::string> p, std::unique_ptr<Ast> b,
            yy::location lc = yy::location())
      : Ast(std::move(lc)), params(std::move(p)), body(std::move(b)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void print(int indent, std::ostream &to) const override;
};

class AstLet : public Ast {
public:
  /* One binding of the let, after its definition has been lifted: the name
   * the body sees, and the lifted function applied to what it captured. */
  struct Binding {
    std::string name;
    std::unique_ptr<Ast> value;
  };

  std::unique_ptr<DefinitionGroup> definitions;
  std::unique_ptr<Ast> in;

  std::vector<Binding> bindings;

  AstLet(std::unique_ptr<DefinitionGroup> d, std::unique_ptr<Ast> i,
         yy::location lc = yy::location())
      : Ast(std::move(lc)), definitions(std::move(d)), in(std::move(i)) {}

  std::shared_ptr<ff::sem::Type> typecheck(ff::sem::TypeManager &mgr) override;

  void findFree(ff::sem::TypeManager &mgr,
                std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                std::set<std::string> &into) override;

  void translate(GlobalScope &scope) override;

  void generate(
      const std::shared_ptr<ff::ir::Enviroment> &env,
      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const override;

  void print(int indent, std::ostream &to) const override;
};
