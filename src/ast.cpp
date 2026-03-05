#include "../include/ast.hpp"

#include "ast.hpp"
#include "enviroment.hpp"
#include "error.hpp"
#include "instructions.hpp"
#include <iostream>
#include <memory>

void printIndent(int n, std::ostream &to) {
  while (n--)
    to << "  ";
}

// ############ Asts ############

std::shared_ptr<ff::sem::Type>
Ast::commonTypecheck(ff::sem::TypeManager &mgr,
                     const ff::sem::TypeContext &context) {
  this->nodeType = this->typecheck(mgr, context);
  return this->nodeType;
}

void Ast::commonResolve(const ff::sem::TypeManager &mgr) {
  ff::sem::TypeVar *var;
  auto resolvedType = mgr.resolve(this->nodeType, var);

  if (var)
    throw ff::TypeError("ambiguous typed program");

  this->resolve(mgr);
  this->nodeType = std::move(resolvedType);
}

void AstInt::resolve(const ff::sem::TypeManager &mgr) const {
  // TODO
}

std::shared_ptr<ff::sem::Type>
AstInt::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeContext &env) const {
  return std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase("Int"));
}

void AstLid::resolve(const ff::sem::TypeManager &mgr) const {
  // TODO
}

void AstInt::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  into.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::PushInt(this->value)));
}

void AstInt::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INT: " << value << std::endl;
}

std::shared_ptr<ff::sem::Type>
AstLid::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeContext &env) const {
  return env.lookup(id);
}

void AstLid::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  into.push_back(std::unique_ptr<ff::ir::Instruction>(
      env->hasVariable(id)
          ? (ff::ir::Instruction *)new ff::ir::Push(env->getOffset(id))
          : (ff::ir::Instruction *)new ff::ir::PushGlobal(id)));
}

void AstLid::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INT: " << id << std::endl;
}

std::shared_ptr<ff::sem::Type>
AstUid::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeContext &env) const {
  return env.lookup(id);
}

void AstUid::resolve(const ff::sem::TypeManager &mgr) const {
  // TODO
}

void AstUid::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  into.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::PushGlobal(this->id)));
}

void AstUid::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INT: " << id << std::endl;
}

std::shared_ptr<ff::sem::Type>
AstBinop::typecheck(ff::sem::TypeManager &mgr,
                    const ff::sem::TypeContext &env) const {
  std::shared_ptr<ff::sem::Type> ltype = left->commonTypecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> rtype = right->commonTypecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> ftype = env.lookup(opName(op));
  if (!ftype)
    throw ff::TypeError(std::string("unknown binary operator ") + opName(op));

  std::shared_ptr<ff::sem::Type> return_type = mgr.newType();
  std::shared_ptr<ff::sem::Type> arrow_one =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(rtype, return_type));
  std::shared_ptr<ff::sem::Type> arrow_two =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(ltype, arrow_one));

  mgr.unify(arrow_two, ftype);
  return return_type;
}

void AstBinop::resolve(const ff::sem::TypeManager &mgr) const {
  left->commonResolve(mgr);
  right->commonResolve(mgr);
}

void AstBinop::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  this->right->generate(env, into);
  this->left->generate(
      std::shared_ptr<ff::ir::Enviroment>(new ff::ir::EnviromentOffset(1, env)),
      into);

  into.push_back(std::unique_ptr<ff::ir::Instruction>(
      new ff::ir::PushGlobal(opAction(op))));
  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
}

void AstBinop::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "BINOP: " << opName(op) << std::endl;
  left->print(indent + 1, to);
  right->print(indent + 1, to);
}

std::shared_ptr<ff::sem::Type>
AstApp::typecheck(ff::sem::TypeManager &mgr,
                  const ff::sem::TypeContext &env) const {
  std::shared_ptr<ff::sem::Type> ltype = left->commonTypecheck(mgr, env);
  std::shared_ptr<ff::sem::Type> rtype = right->commonTypecheck(mgr, env);

  std::shared_ptr<ff::sem::Type> return_type = mgr.newType();
  std::shared_ptr<ff::sem::Type> arrow =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(rtype, return_type));
  mgr.unify(arrow, ltype);
  return return_type;
}

void AstApp::resolve(const ff::sem::TypeManager &mgr) const {
  left->commonResolve(mgr);
  right->commonResolve(mgr);
}

void AstApp::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  this->right->generate(env, into);
  this->left->generate(std::shared_ptr<ff::ir::EnviromentOffset>(
                           new ff::ir::EnviromentOffset(1, env)),
                       into);

  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
}

void AstApp::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "APP:" << std::endl;
  left->print(indent + 1, to);
  right->print(indent + 1, to);
}

void AstCase::resolve(const ff::sem::TypeManager &mgr) const {
  of->commonResolve(mgr);
  for (auto &branch : branches) {
    branch->expr->commonResolve(mgr);
  }
}

std::shared_ptr<ff::sem::Type>
AstCase::typecheck(ff::sem::TypeManager &mgr,
                   const ff::sem::TypeContext &env) const {
  ff::sem::TypeVar *var;
  std::shared_ptr<ff::sem::Type> case_type =
      mgr.resolve(of->commonTypecheck(mgr, env), var);
  std::shared_ptr<ff::sem::Type> branch_type = mgr.newType();

  for (auto &branch : branches) {
    ff::sem::TypeContext new_env = env.scope();

    branch->pattern->match(case_type, mgr, new_env);
    std::shared_ptr<ff::sem::Type> curr_branch_type =
        branch->expr->typecheck(mgr, new_env);

    mgr.unify(branch_type, curr_branch_type);
  }

  case_type = mgr.resolve(case_type, var);
  if (!dynamic_cast<ff::sem::TypeBase *>(case_type.get())) {
    throw ff::TypeError("attempting case analysis of non-data type");
  }

  return branch_type;
}

void AstCase::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  ff::sem::TypeData *type =
      dynamic_cast<ff::sem::TypeData *>(of->nodeType.get());

  of->generate(env, into);
  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::Eval()));

  ff::ir::Jump *jump_instruction = new ff::ir::Jump();

  into.push_back(std::unique_ptr<ff::ir::Instruction>(jump_instruction));

  for (auto &branch : branches) {
    std::vector<std::unique_ptr<ff::ir::Instruction>> branch_instructions;
    PatternVar *vpat;
    PatternConstr *cpat;

    if ((vpat = dynamic_cast<PatternVar *>(branch->pattern.get()))) {
      branch->expr->generate(std::shared_ptr<ff::ir::Enviroment>(
                                 new ff::ir::EnviromentOffset(1, env)),
                             branch_instructions);

      for (auto &constr_pair : type->constructors) {
        if (jump_instruction->tagMappings.find(constr_pair.second.tag) !=
            jump_instruction->tagMappings.end())
          break;

        jump_instruction->tagMappings[constr_pair.second.tag] =
            jump_instruction->branches.size();
      }
      jump_instruction->branches.push_back(std::move(branch_instructions));
    } else if ((cpat = dynamic_cast<PatternConstr *>(branch->pattern.get()))) {
      std::shared_ptr<ff::ir::Enviroment> new_env = env;

      for (auto it = cpat->params.rbegin(); it != cpat->params.rend(); it++) {
        new_env = std::shared_ptr<ff::ir::Enviroment>(
            new ff::ir::EnviromentVar(*it, new_env));
      }

      branch_instructions.push_back(std::unique_ptr<ff::ir::Instruction>(
          new ff::ir::Split(cpat->params.size())));
      branch->expr->generate(new_env, branch_instructions);
      branch_instructions.push_back(std::unique_ptr<ff::ir::Instruction>(
          new ff::ir::Slide(cpat->params.size())));

      int new_tag = type->constructors[cpat->constr].tag;
      if (jump_instruction->tagMappings.find(new_tag) !=
          jump_instruction->tagMappings.end())
        throw ff::TypeError("technically not a type error: duplicate pattern");

      jump_instruction->tagMappings[new_tag] =
          jump_instruction->branches.size();
      jump_instruction->branches.push_back(std::move(branch_instructions));
    }
  }

  for (auto &constr_pair : type->constructors) {
    if (jump_instruction->tagMappings.find(constr_pair.second.tag) ==
        jump_instruction->tagMappings.end())
      throw ff::TypeError("non-total pattern");
  }
}

void AstCase::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "CASE: " << std::endl;
  for (auto &branch : branches) {
    printIndent(indent + 1, to);
    branch->pattern->print(to);
    to << std::endl;
    branch->expr->print(indent + 2, to);
  }
}

void PatternVar::match(std::shared_ptr<ff::sem::Type> t,
                       ff::sem::TypeManager &mgr,
                       ff::sem::TypeContext &env) const {
  env.bind(var, t);
}

void PatternVar::print(std::ostream &to) const { to << var; }

void PatternConstr::match(std::shared_ptr<ff::sem::Type> t,
                          ff::sem::TypeManager &mgr,
                          ff::sem::TypeContext &env) const {
  std::shared_ptr<ff::sem::Type> constructor_type = env.lookup(constr);
  if (!constructor_type)
    throw ff::TypeError(std::string("pattern using unknown constructor ") +
                        constr);

  for (int i = 0; i < params.size(); i++) {
    ff::sem::TypeArr *arr =
        dynamic_cast<ff::sem::TypeArr *>(constructor_type.get());
    if (!arr)
      throw ff::TypeError("too many parameters in constructor pattern");

    env.bind(params[i], arr->left);
    constructor_type = arr->right;
  }

  mgr.unify(t, constructor_type);
}

void PatternConstr::print(std::ostream &to) const {
  to << constr;
  for (auto &param : params) {
    to << " " << param;
  }
}

// ############ Definitions ############

void DefinitionDefn::typeCheckFirst(ff::sem::TypeManager &mgr,
                                    ff::sem::TypeContext &env) {
  this->returnType = mgr.newType();
  std::shared_ptr<ff::sem::Type> fullType = this->returnType;

  for (auto it = this->params.rbegin(); it != this->params.rend(); it++) {
    std::shared_ptr<ff::sem::Type> paramType = mgr.newType();
    fullType = std::shared_ptr<ff::sem::Type>(
        new ff::sem::TypeArr(paramType, fullType));
    this->paramTypes.push_back(paramType);
  }

  env.bind(name, fullType);
}

void DefinitionDefn::typeCheckSecond(ff::sem::TypeManager &mgr,
                                     const ff::sem::TypeContext &env) const {
  ff::sem::TypeContext newEnv = env.scope();
  auto param_it = this->params.begin();
  auto type_it = this->paramTypes.rbegin();

  while (param_it != params.end() && type_it != this->paramTypes.rend()) {
    newEnv.bind(*param_it, *type_it);
    param_it++;
    type_it++;
  }

  std::shared_ptr<ff::sem::Type> body_type = body->commonTypecheck(mgr, newEnv);
  mgr.unify(this->returnType, body_type);
}

void DefinitionData::typeCheckFirst(ff::sem::TypeManager &mgr,
                                    ff::sem::TypeContext &env) {
  ff::sem::TypeData *this_type = new ff::sem::TypeData(name);
  std::shared_ptr<ff::sem::Type> return_type =
      std::shared_ptr<ff::sem::Type>(this_type);
  int next_tag = 0;

  for (auto &constructor : constructors) {
    constructor->tag = next_tag;
    this_type->constructors[constructor->name] = {next_tag++};

    std::shared_ptr<ff::sem::Type> full_type = return_type;

    for (auto it = constructor->types.rbegin(); it != constructor->types.rend();
         it++) {
      std::shared_ptr<ff::sem::Type> type =
          std::shared_ptr<ff::sem::Type>(new ff::sem::TypeBase(*it));
      full_type =
          std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(type, full_type));
    }

    env.bind(constructor->name, full_type);
  }
}

void DefinitionDefn::resolve(const ff::sem::TypeManager &mgr) {
  ff::sem::TypeVar *var;
  body->commonResolve(mgr);

  this->returnType = mgr.resolve(this->returnType, var);

  if (var)
    throw ff::TypeError("ambiguously typed program");

  for (auto &paramType : this->paramTypes) {
    paramType = mgr.resolve(paramType, var);

    if (var)
      throw ff::TypeError("ambiguously typed program");
  }
}

void DefinitionDefn::generate() {
  auto new_env = std::shared_ptr<ff::ir::Enviroment>(
      new ff::ir::EnviromentOffset(0, nullptr));

  for (auto it = params.rbegin(); it != params.rend(); it++) {
    new_env = std::shared_ptr<ff::ir::Enviroment>(
        new ff::ir::EnviromentVar(*it, new_env));
  }

  body->generate(new_env, this->instructions);
  this->instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(params.size())));
  this->instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Pop(params.size())));
}

void DefinitionDefn::generateLLVMFirst(ff::cg::CodeGenerator &generator) {
  this->generatedFunction =
      generator.createCustomFunction(this->name, this->params.size());
}

void DefinitionDefn::generateLLVMSecond(ff::cg::CodeGenerator &generator) {
  generator.builder.SetInsertPoint(&this->generatedFunction->getEntryBlock());
  for (auto &instruction : this->instructions) {
    instruction->generate(generator, this->generatedFunction);
  }

  generator.builder.CreateRetVoid();
}

void DefinitionData::resolve(const ff::sem::TypeManager &mgr) {
  // TODO
}

void DefinitionData::typeCheckSecond(ff::sem::TypeManager &mgr,
                                     const ff::sem::TypeContext &env) const {
  // TODO
}

void DefinitionData::generate() {
  // TODO
}

void DefinitionData::generateLLVMFirst(ff::cg::CodeGenerator &generator) {
  for (auto &constructor : this->constructors) {
    auto newFunction = generator.createCustomFunction(
        constructor->name, constructor->types.size());

    generator.builder.SetInsertPoint(&newFunction->getEntryBlock());
    generator.createPack(newFunction,
                         generator.createSize(constructor->types.size()),
                         generator.createI8(constructor->tag));
    generator.builder.CreateRetVoid();
  }
}

void DefinitionData::generateLLVMSecond(ff::cg::CodeGenerator &generator) {}
