#pragma once

#include "file_manager.hpp"
#include "types.hpp"
#include <exception>
#include <optional>
#include <ostream>
#include <string>

namespace ff {

/* Not every error can name a place in the source: a bad command line, or a
 * failure to write the object file, happen outside of any program text. */
using MaybeLocation = std::optional<yy::location>;

class CompilerError : public std::exception {
private:
  std::string description;
  MaybeLocation loc;

protected:
  void printAbout(std::ostream &to) const;
  void printLocation(std::ostream &to, const drv::FileManager &fm,
                     bool highlight) const;

public:
  CompilerError(std::string d, MaybeLocation l = std::nullopt)
      : description(std::move(d)), loc(std::move(l)) {}

  const char *what() const noexcept override;
  virtual void prettyPrint(std::ostream &to, const drv::FileManager &fm) const;
};

class TypeError : public CompilerError {
public:
  TypeError(std::string d, MaybeLocation l = std::nullopt)
      : CompilerError(std::move(d), std::move(l)) {}

  const char *what() const noexcept override;
  void prettyPrint(std::ostream &to, const drv::FileManager &fm) const override;
};

class UnificationError : public TypeError {
private:
  std::shared_ptr<ff::sem::Type> left;
  std::shared_ptr<ff::sem::Type> right;

public:
  UnificationError(std::shared_ptr<ff::sem::Type> l,
                   std::shared_ptr<ff::sem::Type> r,
                   MaybeLocation loc = std::nullopt)
      : TypeError("failed to unify types", std::move(loc)), left(std::move(l)),
        right(std::move(r)) {}

  using TypeError::prettyPrint;
  void prettyPrint(std::ostream &to, const drv::FileManager &fm,
                   const ff::sem::TypeManager &mgr) const;
};

class CliError : public std::exception {
public:
  std::string description;

  CliError(std::string _description) : description(std::move(_description)) {}

  const char *what() const noexcept override;
};

} // namespace ff
