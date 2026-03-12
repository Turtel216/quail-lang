#pragma once

#include "context.hpp"
#include "types.hpp"
#include <set>

namespace ff {
namespace sem {

class ParsedType {
public:
  virtual ~ParsedType() = default;

  virtual std::shared_ptr<Type> toType(const std::set<std::string> &vars,
                                       const TypeContext &typeCtx) const = 0;
};

class ParsedTypeApp : public ParsedType {
public:
  std::string name;
  std::vector<std::unique_ptr<ParsedType>> arguments;

  ParsedTypeApp(std::string _name,
                std::vector<std::unique_ptr<ParsedType>> _arguments)
      : name(std::move(_name)), arguments(std::move(_arguments)) {}

  std::shared_ptr<Type> toType(const std::set<std::string> &vars,
                               const TypeContext &typeCtx) const override;
};

class ParsedTypeVar : public ParsedType {
public:
  std::string var;

  ParsedTypeVar(std::string _var) : var(std::move(_var)) {}

  std::shared_ptr<Type> toType(const std::set<std::string> &vars,
                               const TypeContext &typeCtx) const override;
};

class ParsedTypeArr : public ParsedType {
public:
  std::unique_ptr<ParsedType> left;
  std::unique_ptr<ParsedType> right;

  ParsedTypeArr(std::unique_ptr<ParsedType> _left,
                std::unique_ptr<ParsedType> _right)
      : left(std::move(_left)), right(std::move(_right)) {}

  std::shared_ptr<Type> toType(const std::set<std::string> &vars,
                               const TypeContext &typeCtx) const override;
};
} // namespace sem
} // namespace ff
