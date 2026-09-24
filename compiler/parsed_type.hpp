#pragma once

#include "context.hpp"
#include "types.hpp"
#include <location.hh>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace ff {
namespace sem {

class ParsedType {
public:
  virtual ~ParsedType() = default;

  /* `loc` is the definition this type was written in, so a bad type can be
   * reported against the source that declared it. */
  virtual std::shared_ptr<Type> toType(const std::set<std::string> &vars,
                                       const TypeContext &typeCtx,
                                       const yy::location &loc) const = 0;

  /* Collect the lowercase names this type mentions. A signature quantifies
   * over the ones it writes, so they have to be gathered before any part of
   * it is turned into a type. */
  virtual void collectVariables(std::set<std::string> &into) const = 0;

  /* Write the type back out as the source that would parse to it again. */
  virtual void printSource(std::ostream &to) const = 0;
};

class ParsedTypeApp : public ParsedType {
public:
  std::string name;
  std::vector<std::unique_ptr<ParsedType>> arguments;

  ParsedTypeApp(std::string _name,
                std::vector<std::unique_ptr<ParsedType>> _arguments)
      : name(std::move(_name)), arguments(std::move(_arguments)) {}

  std::shared_ptr<Type> toType(const std::set<std::string> &vars,
                               const TypeContext &typeCtx,
                               const yy::location &loc) const override;

  void collectVariables(std::set<std::string> &into) const override;

  void printSource(std::ostream &to) const override;
};

class ParsedTypeVar : public ParsedType {
public:
  std::string var;

  ParsedTypeVar(std::string _var) : var(std::move(_var)) {}

  std::shared_ptr<Type> toType(const std::set<std::string> &vars,
                               const TypeContext &typeCtx,
                               const yy::location &loc) const override;

  void collectVariables(std::set<std::string> &into) const override;

  void printSource(std::ostream &to) const override;
};

class ParsedTypeArr : public ParsedType {
public:
  std::unique_ptr<ParsedType> left;
  std::unique_ptr<ParsedType> right;

  ParsedTypeArr(std::unique_ptr<ParsedType> _left,
                std::unique_ptr<ParsedType> _right)
      : left(std::move(_left)), right(std::move(_right)) {}

  std::shared_ptr<Type> toType(const std::set<std::string> &vars,
                               const TypeContext &typeCtx,
                               const yy::location &loc) const override;

  void collectVariables(std::set<std::string> &into) const override;

  void printSource(std::ostream &to) const override;
};

/* One constraint as the program wrote it: a class name and the type it is
 * claimed about. It stays unresolved until the class environment is built,
 * so that a class named before it is declared is still an ordinary forward
 * reference. */
class ParsedPred {
public:
  std::string className;
  std::vector<std::unique_ptr<ParsedType>> arguments;
  yy::location loc;

  ParsedPred(std::string _className,
             std::vector<std::unique_ptr<ParsedType>> _arguments,
             yy::location lc = yy::location())
      : className(std::move(_className)), arguments(std::move(_arguments)),
        loc(std::move(lc)) {}

  void collectVariables(std::set<std::string> &into) const;
  void printSource(std::ostream &to) const;
};

/* A written context, as it appears to the left of a =>. Empty when the
 * definition it belongs to did not write one. */
using ParsedContext = std::vector<std::unique_ptr<ParsedPred>>;

void printContextSource(const ParsedContext &context, std::ostream &to);
} // namespace sem
} // namespace ff
