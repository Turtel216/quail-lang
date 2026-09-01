#pragma once

#include <memory>
#include <string>
namespace ff {
namespace ir {

class Enviroment {
public:
  virtual ~Enviroment() = default;

  virtual int getOffset(const std::string &name) const = 0;
  virtual bool hasVariable(const std::string &name) const = 0;
};

class EnviromentVar : public Enviroment {
private:
  std::string name;
  std::shared_ptr<Enviroment> parent;

public:
  EnviromentVar(std::string _name, std::shared_ptr<Enviroment> _parent)
      : name(std::move(_name)), parent(std::move(_parent)) {}

  int getOffset(const std::string &name) const override;
  bool hasVariable(const std::string &name) const override;
};

class EnviromentOffset : public Enviroment {
private:
  int offset;
  std::shared_ptr<Enviroment> parent;

public:
  EnviromentOffset(int _offset, std::shared_ptr<Enviroment> _parent)
      : offset(_offset), parent(std::move(_parent)) {}

  int getOffset(const std::string &name) const override;
  bool hasVariable(const std::string &name) const override;
};
} // namespace ir
} // namespace ff
