#include "../include/ast.hpp"

#include "../include/context.hpp"
#include "../include/enviroment.hpp"
#include "../include/types.hpp"
#include "error.hpp"
#include "instructions.hpp"
#include <iostream>
#include <memory>

void printIndent(int n, std::ostream &to) {
  while (n--)
    to << "  ";
}

// ############ Asts ############

void AstInt::findFree(ff::sem::TypeManager &mgr,
                      std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                      std::set<std::string> &into) {
  this->typeContext = typeCtx;
}

std::shared_ptr<ff::sem::Type> AstInt::typecheck(ff::sem::TypeManager &mgr) {
  return std::shared_ptr<ff::sem::Type>(
      new ff::sem::TypeApp(typeContext->lookupType("Int")));
}

void AstLid::findFree(ff::sem::TypeManager &mgr,
                      std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                      std::set<std::string> &into) {
  this->typeContext = typeCtx;
  if (typeCtx->lookup(id) == nullptr)
    into.insert(id);
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

std::shared_ptr<ff::sem::Type> AstLid::typecheck(ff::sem::TypeManager &mgr) {
  return typeContext->lookup(id)->instantiate(mgr);
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

std::shared_ptr<ff::sem::Type> AstUid::typecheck(ff::sem::TypeManager &mgr) {
  return typeContext->lookup(id)->instantiate(mgr);
}

void AstUid::findFree(ff::sem::TypeManager &mgr,
                      std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                      std::set<std::string> &into) {
  this->typeContext = typeCtx;
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

std::shared_ptr<ff::sem::Type> AstBinop::typecheck(ff::sem::TypeManager &mgr) {
  auto ltype = left->typecheck(mgr);
  auto rtype = right->typecheck(mgr);
  auto ftype = typeContext->lookup(opName(op))->instantiate(mgr);
  if (!ftype)
    throw ff::TypeError(std::string("unknown binary operator ") + opName(op));

  auto returnType = mgr.newType();
  auto arrowOne =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(rtype, returnType));
  auto arrowTwo =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(ltype, arrowOne));

  mgr.unify(arrowTwo, ftype);
  return returnType;
}

void AstBinop::findFree(ff::sem::TypeManager &mgr,
                        std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                        std::set<std::string> &into) {
  this->typeContext = typeCtx;
  left->findFree(mgr, typeCtx, into);
  right->findFree(mgr, typeCtx, into);
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

std::shared_ptr<ff::sem::Type> AstApp::typecheck(ff::sem::TypeManager &mgr) {
  auto ltype = left->typecheck(mgr);
  auto rtype = right->typecheck(mgr);

  auto returnType = mgr.newType();
  auto arrow =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(rtype, returnType));
  mgr.unify(arrow, ltype);
  return returnType;
}

void AstApp::findFree(ff::sem::TypeManager &mgr,
                      std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                      std::set<std::string> &into) {
  this->typeContext = typeCtx;
  left->findFree(mgr, typeCtx, into);
  right->findFree(mgr, typeCtx, into);
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

void AstCase::findFree(ff::sem::TypeManager &mgr,
                       std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                       std::set<std::string> &into) {
  this->typeContext = typeCtx;
  of->findFree(mgr, typeCtx, into);
  for (auto &branch : branches) {
    auto newEnv = ff::sem::typeScope(typeCtx);
    branch->pattern->insertBindings(mgr, newEnv);
    branch->expr->findFree(mgr, newEnv, into);
  }
}

std::shared_ptr<ff::sem::Type> AstCase::typecheck(ff::sem::TypeManager &mgr) {
  ff::sem::TypeVar *var;
  auto caseType = mgr.resolve(of->typecheck(mgr), var);
  auto branchType = mgr.newType();

  for (auto &branch : branches) {
    branch->pattern->typecheck(caseType, mgr, branch->expr->typeContext);
    auto currBranchType = branch->expr->typecheck(mgr);
    mgr.unify(branchType, currBranchType);
  }

  this->inputType = mgr.resolve(caseType, var);
  ff::sem::TypeApp *appType;
  if (!(appType = dynamic_cast<ff::sem::TypeApp *>(inputType.get())) ||
      !dynamic_cast<ff::sem::TypeData *>(appType->constructor.get())) {
    throw ff::TypeError("attempting case analysis of non-data type");
  }

  return branchType;
}

void AstCase::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  ff::sem::TypeApp *appType = dynamic_cast<ff::sem::TypeApp *>(inputType.get());
  ff::sem::TypeData *type =
      dynamic_cast<ff::sem::TypeData *>(appType->constructor.get());

  of->generate(env, into);
  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::Eval()));

  ff::ir::Jump *jumpInstruction = new ff::ir::Jump();

  into.push_back(std::unique_ptr<ff::ir::Instruction>(jumpInstruction));

  for (auto &branch : branches) {
    std::vector<std::unique_ptr<ff::ir::Instruction>> branchInstructions;
    PatternVar *vpat;
    PatternConstr *cpat;

    if ((vpat = dynamic_cast<PatternVar *>(branch->pattern.get()))) {
      branch->expr->generate(std::shared_ptr<ff::ir::Enviroment>(
                                 new ff::ir::EnviromentOffset(1, env)),
                             branchInstructions);

      for (auto &constrPair : type->constructors) {
        if (jumpInstruction->tagMappings.find(constrPair.second.tag) !=
            jumpInstruction->tagMappings.end())
          break;

        jumpInstruction->tagMappings[constrPair.second.tag] =
            jumpInstruction->branches.size();
      }
      jumpInstruction->branches.push_back(std::move(branchInstructions));
    } else if ((cpat = dynamic_cast<PatternConstr *>(branch->pattern.get()))) {
      std::shared_ptr<ff::ir::Enviroment> newEnv = env;

      for (auto it = cpat->params.rbegin(); it != cpat->params.rend(); it++) {
        newEnv = std::shared_ptr<ff::ir::Enviroment>(
            new ff::ir::EnviromentVar(*it, newEnv));
      }

      branchInstructions.push_back(std::unique_ptr<ff::ir::Instruction>(
          new ff::ir::Split(cpat->params.size())));
      branch->expr->generate(newEnv, branchInstructions);
      branchInstructions.push_back(std::unique_ptr<ff::ir::Instruction>(
          new ff::ir::Slide(cpat->params.size())));

      int newTag = type->constructors[cpat->constr].tag;
      if (jumpInstruction->tagMappings.find(newTag) !=
          jumpInstruction->tagMappings.end())
        throw ff::TypeError("technically not a type error: duplicate pattern");

      jumpInstruction->tagMappings[newTag] =
          jumpInstruction->branches.size();
      jumpInstruction->branches.push_back(std::move(branchInstructions));
    }
  }

  for (auto &constrPair : type->constructors) {
    if (jumpInstruction->tagMappings.find(constrPair.second.tag) ==
        jumpInstruction->tagMappings.end())
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

void PatternVar::print(std::ostream &to) const { to << var; }

void PatternVar::insertBindings(
    ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) const {
  typeCtx->bind(var, mgr.newType());
}

void PatternVar::typecheck(
    std::shared_ptr<ff::sem::Type> t, ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) const {
  mgr.unify(typeCtx->lookup(var)->instantiate(mgr), t);
}

void PatternConstr::typecheck(
    std::shared_ptr<ff::sem::Type> t, ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) const {
  auto constructorType = typeCtx->lookup(constr)->instantiate(mgr);
  if (!constructorType) {
    throw ff::TypeError(std::string("pattern using unknown constructor ") +
                        constr);
  }

  for (auto &param : params) {
    ff::sem::TypeArr *arr =
        dynamic_cast<ff::sem::TypeArr *>(constructorType.get());

    if (!arr)
      throw ff::TypeError("too many parameters in constructor pattern");

    mgr.unify(typeCtx->lookup(param)->instantiate(mgr), arr->getLeft());
    constructorType = arr->getRight();
  }

  mgr.unify(t, constructorType);
}

void PatternConstr::insertBindings(
    ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) const {
  for (auto &param : this->params) {
    typeCtx->bind(param, mgr.newType());
  }
}

void PatternConstr::print(std::ostream &to) const {
  to << constr;
  for (auto &param : params) {
    to << " " << param;
  }
}

// ############ Definitions ############

void DefinitionDefn::findFree(ff::sem::TypeManager &mgr,
                              std::shared_ptr<ff::sem::TypeContext> &typeCtx) {
  this->typeContext = typeCtx;

  varContext = ff::sem::typeScope(typeCtx);
  returnType = mgr.newType();
  fullType = returnType;

  for (auto it = params.rbegin(); it != params.rend(); it++) {
    auto paramType = mgr.newType();
    fullType = std::shared_ptr<ff::sem::Type>(
        new ff::sem::TypeArr(paramType, fullType));
    varContext->bind(*it, paramType);
  }

  body->findFree(mgr, varContext, freeVariables);
}

void DefinitionDefn::insertTypes(ff::sem::TypeManager &mgr) {
  typeContext->bind(name, fullType);
}

void DefinitionDefn::typecheck(ff::sem::TypeManager &mgr) {
  auto bodyType = body->typecheck(mgr);
  mgr.unify(returnType, bodyType);
}

void DefinitionDefn::compile() {
  auto newEnv = std::shared_ptr<ff::ir::Enviroment>(
      new ff::ir::EnviromentOffset(0, nullptr));

  for (auto it = params.rbegin(); it != params.rend(); it++) {
    newEnv = std::shared_ptr<ff::ir::Enviroment>(
        new ff::ir::EnviromentVar(*it, newEnv));
  }
  body->generate(newEnv, instructions);
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(params.size())));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Pop(params.size())));
}

void DefinitionDefn::declareLLVM(ff::cg::CodeGenerator &generator) {
  generatedFunction = generator.createCustomFunction(name, params.size());
}

void DefinitionDefn::generateLLVM(ff::cg::CodeGenerator &generator) {
  generator.builder.SetInsertPoint(&generatedFunction->getEntryBlock());
  for (auto &instruction : instructions) {
    instruction->generate(generator, generatedFunction);
  }
  generator.builder.CreateRetVoid();
}

void DefinitionData::insertTypes(
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) {
  this->typeContext = typeCtx;
  typeContext->bindType(name, std::shared_ptr<ff::sem::Type>(
                                  new ff::sem::TypeData(name, vars.size())));
}

void DefinitionData::insertConstructors() const {
  auto thisTypePtr = typeContext->lookupType(name);
  ff::sem::TypeData *thisType =
      static_cast<ff::sem::TypeData *>(thisTypePtr.get());

  int nextTag = 0;

  std::set<std::string> varSet;
  ff::sem::TypeApp *returnApp = new ff::sem::TypeApp(std::move(thisTypePtr));
  std::shared_ptr<ff::sem::Type> returnType(returnApp);

  for (auto &var : vars) {
    if (varSet.find(var) != varSet.end())
      ff::DebugError("DefinitionData::insertConstructors");

    varSet.insert(var);
    returnApp->arguments.push_back(
        std::shared_ptr<ff::sem::Type>(new ff::sem::TypeVar(var)));
  }

  for (auto &constructor : constructors) {
    constructor->tag = nextTag;
    thisType->constructors[constructor->name] = {nextTag++};

    std::shared_ptr<ff::sem::Type> fullType = returnType;
    for (auto it = constructor->types.rbegin(); it != constructor->types.rend();
         it++) {
      std::shared_ptr<ff::sem::Type> type = (*it)->toType(varSet, typeContext);
      fullType =
          std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(type, fullType));
    }

    std::shared_ptr<ff::sem::TypeScheme> fullScheme(
        new ff::sem::TypeScheme(std::move(fullType)));

    fullScheme->forall.insert(fullScheme->forall.begin(), vars.begin(),
                              vars.end());

    typeContext->bind(constructor->name, fullScheme);
  }
}

void DefinitionData::generateLLVM(ff::cg::CodeGenerator &generator) {
  for (auto &constructor : constructors) {
    auto newFunction = generator.createCustomFunction(
        constructor->name, constructor->types.size());

    std::vector<std::unique_ptr<ff::ir::Instruction>> instructions;

    instructions.push_back(std::unique_ptr<ff::ir::Instruction>(
        new ff::ir::Pack(constructor->tag, constructor->types.size())));

    instructions.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(0)));

    generator.builder.SetInsertPoint(&newFunction->getEntryBlock());
    for (auto &instruction : instructions) {
      instruction->generate(generator, newFunction);
    }

    generator.builder.CreateRetVoid();
  }
}
