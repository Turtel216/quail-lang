#pragma once

#include "types.hpp"
#include <exception>

namespace ff {

class TypeError : public std::exception {
public:
  std::string description;

  TypeError(std::string d) : description(std::move(d)) {}

  const char *what() const noexcept override;
};

class UnificationError : public TypeError {
public:
  std::shared_ptr<ff::sem::Type> left;
  std::shared_ptr<ff::sem::Type> right;

  UnificationError(std::shared_ptr<ff::sem::Type> l,
                   std::shared_ptr<ff::sem::Type> r)
      : TypeError("failed to unify types"), left(std::move(l)),
        right(std::move(r)) {}
};

class CliError : public std::exception {
public:
  std::string description;

  CliError(std::string _description) : description(std::move(_description)) {}

  const char *what() const noexcept override;
};

class DebugError : public std::exception {
public:
  std::string description;

  DebugError(std::string _description) : description(std::move(_description)) {}

  const char *what() const noexcept override;
};

} // namespace ff
