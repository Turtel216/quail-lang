#include "ast.hpp"

#include "context.hpp"
#include "enviroment.hpp"
#include "error.hpp"
#include "instructions.hpp"
#include "types.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>

void printIndent(int n, std::ostream &to) {
  while (n--)
    to << "  ";
}

namespace {

/* What a lifted definition looks like from the outside: the new global
 * applied to each of the variables it captured, in the same order they were
 * prepended to its parameter list. */
std::unique_ptr<Ast> partialApplication(const DefinitionDefn &definition) {
  AstLid *global = new AstLid(definition.mangledName);
  global->lifted = true;

  std::unique_ptr<Ast> application(global);

  for (auto &captured : definition.capturedVariables) {
    application = std::unique_ptr<Ast>(new AstApp(
        std::move(application), std::unique_ptr<Ast>(new AstLid(captured))));
  }

  return application;
}

} // namespace

// ############ Asts ############

void AstInt::findFree(ff::sem::TypeManager &,
                      std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                      std::set<std::string> &) {
  this->typeContext = typeCtx;
}

std::shared_ptr<ff::sem::Type> AstInt::typecheck(ff::sem::TypeManager &) {
  return std::shared_ptr<ff::sem::Type>(
      new ff::sem::TypeApp(typeContext->lookupType("Int")));
}

void AstInt::translate(GlobalScope &) {}

void AstLid::findFree(ff::sem::TypeManager &,
                      std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                      std::set<std::string> &into) {
  this->typeContext = typeCtx;
  into.insert(id);
}

void AstInt::generate(
    const std::shared_ptr<ff::ir::Enviroment> &,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  into.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::PushInt(this->value)));
}

void AstInt::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INT: " << value << std::endl;
}

std::shared_ptr<ff::sem::Type> AstLid::typecheck(ff::sem::TypeManager &mgr) {
  auto variable = typeContext->lookup(id);
  /* Undefined names are reported before typechecking begins. */
  assert(variable != nullptr);
  return variable->scheme->instantiate(mgr);
}

void AstLid::translate(GlobalScope &) {}

void AstLid::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  /* A lifted reference names its global outright; it was created after
   * typechecking and so has no scope to resolve the name against. */
  if (lifted) {
    into.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::PushGlobal(id)));
    return;
  }

  into.push_back(std::unique_ptr<ff::ir::Instruction>(
      env->hasVariable(id)
          ? (ff::ir::Instruction *)new ff::ir::Push(env->getOffset(id))
          : (ff::ir::Instruction *)new ff::ir::PushGlobal(
                this->typeContext->getMangledName(id))));
}

void AstLid::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INT: " << id << std::endl;
}

std::shared_ptr<ff::sem::Type> AstUid::typecheck(ff::sem::TypeManager &mgr) {
  auto constructor = typeContext->lookup(id);
  if (!constructor)
    throw ff::TypeError("unknown constructor " + id, loc);
  return constructor->scheme->instantiate(mgr);
}

void AstUid::findFree(ff::sem::TypeManager &,
                      std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                      std::set<std::string> &) {
  this->typeContext = typeCtx;
}

void AstUid::translate(GlobalScope &) {}

void AstUid::generate(
    const std::shared_ptr<ff::ir::Enviroment> &,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  into.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::PushGlobal(this->id)));
}

void AstUid::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INT: " << id << std::endl;
}

std::shared_ptr<ff::sem::Type> AstList::typecheck(ff::sem::TypeManager &mgr) {
  auto itemType = mgr.newType();
  for (auto &item : items) {
    mgr.unify(itemType, item->typecheck(mgr), item->loc);
  }

  ff::sem::TypeApp *listApp =
      new ff::sem::TypeApp(typeContext->lookupType(ff::sem::listTypeName));
  std::shared_ptr<ff::sem::Type> listType(listApp);
  listApp->arguments.push_back(std::move(itemType));

  return listType;
}

void AstList::findFree(ff::sem::TypeManager &mgr,
                       std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                       std::set<std::string> &into) {
  this->typeContext = typeCtx;
  for (auto &item : items) {
    item->findFree(mgr, typeCtx, into);
  }
}

void AstList::translate(GlobalScope &scope) {
  for (auto &item : items) {
    item->translate(scope);
  }
}

void AstList::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  into.push_back(std::unique_ptr<ff::ir::Instruction>(
      new ff::ir::PushGlobal(ff::sem::listNilName)));

  /* Built back to front, so that the tail an item is consed onto is already
   * the single slot sitting on top of the stack. */
  for (auto it = items.rbegin(); it != items.rend(); it++) {
    (*it)->generate(std::shared_ptr<ff::ir::Enviroment>(
                        new ff::ir::EnviromentOffset(1, env)),
                    into);

    into.push_back(std::unique_ptr<ff::ir::Instruction>(
        new ff::ir::PushGlobal(ff::sem::listConsName)));
    into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
    into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
  }
}

void AstList::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "LIST:" << std::endl;
  for (auto &item : items) {
    item->print(indent + 1, to);
  }
}

std::shared_ptr<ff::sem::Type> AstBinop::typecheck(ff::sem::TypeManager &mgr) {
  auto ltype = left->typecheck(mgr);
  auto rtype = right->typecheck(mgr);
  auto opVariable = typeContext->lookup(opName(op));
  if (!opVariable)
    throw ff::TypeError(std::string("unknown binary operator ") + opName(op),
                        loc);

  auto ftype = opVariable->scheme->instantiate(mgr);
  auto returnType = mgr.newType();
  auto arrowOne =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(rtype, returnType));
  auto arrowTwo =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(ltype, arrowOne));

  mgr.unify(ftype, arrowTwo, loc);
  return returnType;
}

void AstBinop::findFree(ff::sem::TypeManager &mgr,
                        std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                        std::set<std::string> &into) {
  this->typeContext = typeCtx;
  left->findFree(mgr, typeCtx, into);
  right->findFree(mgr, typeCtx, into);
}

void AstBinop::translate(GlobalScope &scope) {
  left->translate(scope);
  right->translate(scope);
}

void AstBinop::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  this->right->generate(env, into);
  this->left->generate(
      std::shared_ptr<ff::ir::Enviroment>(new ff::ir::EnviromentOffset(1, env)),
      into);

  into.push_back(std::unique_ptr<ff::ir::Instruction>(
      new ff::ir::PushGlobal(this->typeContext->getMangledName(opName(op)))));
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
  mgr.unify(ltype, arrow, loc);
  return returnType;
}

void AstApp::findFree(ff::sem::TypeManager &mgr,
                      std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                      std::set<std::string> &into) {
  this->typeContext = typeCtx;
  left->findFree(mgr, typeCtx, into);
  right->findFree(mgr, typeCtx, into);
}

void AstApp::translate(GlobalScope &scope) {
  left->translate(scope);
  right->translate(scope);
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

std::shared_ptr<ff::sem::Type> AstPipe::typecheck(ff::sem::TypeManager &mgr) {
  auto valueType = value->typecheck(mgr);
  auto functionType = function->typecheck(mgr);

  /* A right side already known not to be a function is worth saying so
   * outright; unifying it would only report a mismatch against an arrow the
   * program never wrote. */
  ff::sem::TypeVar *var;
  auto resolved = mgr.resolve(functionType, var);
  if (!var && !dynamic_cast<ff::sem::TypeArr *>(resolved.get())) {
    std::ostringstream errorStream;
    errorStream << "the right side of |> is not a function, its type is ";
    resolved->print(mgr, errorStream);

    throw ff::TypeError(errorStream.str(), function->loc);
  }

  auto returnType = mgr.newType();
  auto arrow = std::shared_ptr<ff::sem::Type>(
      new ff::sem::TypeArr(valueType, returnType));

  mgr.unify(functionType, arrow, loc);
  return returnType;
}

void AstPipe::findFree(ff::sem::TypeManager &mgr,
                       std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                       std::set<std::string> &into) {
  this->typeContext = typeCtx;
  value->findFree(mgr, typeCtx, into);
  function->findFree(mgr, typeCtx, into);
}

void AstPipe::translate(GlobalScope &scope) {
  value->translate(scope);
  function->translate(scope);
}

void AstPipe::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  /* The argument goes down first, exactly as for a written-out application;
   * only the two sides trade places. */
  this->value->generate(env, into);
  this->function->generate(std::shared_ptr<ff::ir::Enviroment>(
                               new ff::ir::EnviromentOffset(1, env)),
                           into);

  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
}

void AstPipe::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "PIPE:" << std::endl;
  value->print(indent + 1, to);
  function->print(indent + 1, to);
}

void AstCase::findFree(ff::sem::TypeManager &mgr,
                       std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                       std::set<std::string> &into) {
  this->typeContext = typeCtx;
  of->findFree(mgr, typeCtx, into);
  for (auto &branch : branches) {
    auto newEnv = ff::sem::typeScope(typeCtx);
    branch->pattern->insertBindings(mgr, newEnv);

    std::set<std::string> branchFree;
    branch->expr->findFree(mgr, newEnv, branchFree);
    branch->pattern->eraseBindings(branchFree);

    into.insert(branchFree.begin(), branchFree.end());
  }
}

void AstCase::translate(GlobalScope &scope) {
  of->translate(scope);
  for (auto &branch : branches) {
    branch->expr->translate(scope);
  }
}

std::shared_ptr<ff::sem::Type> AstCase::typecheck(ff::sem::TypeManager &mgr) {
  ff::sem::TypeVar *var;
  auto caseType = mgr.resolve(of->typecheck(mgr), var);
  auto branchType = mgr.newType();

  for (auto &branch : branches) {
    branch->pattern->typecheck(caseType, mgr, branch->expr->typeContext);
    auto currBranchType = branch->expr->typecheck(mgr);
    mgr.unify(branchType, currBranchType, branch->expr->loc);
  }

  this->inputType = mgr.resolve(caseType, var);
  ff::sem::TypeApp *appType;
  if (!(appType = dynamic_cast<ff::sem::TypeApp *>(inputType.get())) ||
      !dynamic_cast<ff::sem::TypeData *>(appType->constructor.get())) {
    throw ff::TypeError("attempting case analysis of non-data type", loc);
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
        throw ff::CompilerError("duplicate pattern in case expression",
                                branch->pattern->loc);

      jumpInstruction->tagMappings[newTag] = jumpInstruction->branches.size();
      jumpInstruction->branches.push_back(std::move(branchInstructions));
    }
  }

  for (auto &constrPair : type->constructors) {
    if (jumpInstruction->tagMappings.find(constrPair.second.tag) ==
        jumpInstruction->tagMappings.end())
      throw ff::CompilerError("case expression does not cover every "
                              "constructor of its data type",
                              loc);
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

std::shared_ptr<ff::sem::Type> AstLambda::typecheck(ff::sem::TypeManager &mgr) {
  mgr.unify(returnType, body->typecheck(mgr), body->loc);
  return fullType;
}

void AstLambda::findFree(ff::sem::TypeManager &mgr,
                         std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                         std::set<std::string> &into) {
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
  for (auto &param : params) {
    freeVariables.erase(param);
  }

  into.insert(freeVariables.begin(), freeVariables.end());
}

void AstLambda::translate(GlobalScope &scope) {
  lifted = std::unique_ptr<DefinitionDefn>(
      new DefinitionDefn("lambda", params, std::move(body), loc));

  lifted->visibility = ff::sem::Visibility::Local;
  lifted->typeContext = typeContext;
  lifted->freeVariables = freeVariables;
  lifted->translate(scope);

  translated = partialApplication(*lifted);
}

void AstLambda::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  translated->generate(env, into);
}

void AstLambda::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "LAMBDA:";
  for (auto &param : params) {
    to << " " << param;
  }
  to << std::endl;
  body->print(indent + 1, to);
}

std::shared_ptr<ff::sem::Type> AstLet::typecheck(ff::sem::TypeManager &mgr) {
  definitions->typecheck(mgr);
  return in->typecheck(mgr);
}

void AstLet::findFree(ff::sem::TypeManager &mgr,
                      std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                      std::set<std::string> &into) {
  this->typeContext = ff::sem::typeScope(typeCtx);

  definitions->findFree(mgr, this->typeContext, ff::sem::Visibility::Local,
                        into);

  std::set<std::string> bodyFree;
  in->findFree(mgr, this->typeContext, bodyFree);
  for (auto &pair : definitions->defsDefn) {
    bodyFree.erase(pair.first);
  }

  into.insert(bodyFree.begin(), bodyFree.end());
}

void AstLet::translate(GlobalScope &scope) {
  definitions->translate(scope);

  for (auto &pair : definitions->defsDefn) {
    bindings.push_back({pair.first, partialApplication(*pair.second)});
  }

  in->translate(scope);
}

void AstLet::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  std::shared_ptr<ff::ir::Enviroment> newEnv = env;
  for (auto &binding : bindings) {
    newEnv = std::shared_ptr<ff::ir::Enviroment>(
        new ff::ir::EnviromentVar(binding.name, newEnv));
  }

  /* One placeholder indirection per binding, filled in below. Allocating
   * them up front is what lets a binding refer to itself or to a sibling:
   * the partial applications capture the placeholder nodes, and Update
   * rewrites those same nodes in place. */
  into.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Alloc(bindings.size())));

  for (auto &binding : bindings) {
    binding.value->generate(newEnv, into);
    into.push_back(std::unique_ptr<ff::ir::Instruction>(
        new ff::ir::Update(newEnv->getOffset(binding.name))));
  }

  in->generate(newEnv, into);
  into.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Slide(bindings.size())));
}

void AstLet::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "LET:" << std::endl;
  for (auto &pair : definitions->defsDefn) {
    printIndent(indent + 1, to);
    to << pair.first;
    for (auto &param : pair.second->params) {
      to << " " << param;
    }
    to << ":" << std::endl;
    pair.second->body->print(indent + 2, to);
  }
  printIndent(indent, to);
  to << "IN:" << std::endl;
  in->print(indent + 1, to);
}

void PatternVar::print(std::ostream &to) const { to << var; }

void PatternVar::insertBindings(
    ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) const {
  typeCtx->bind(var, mgr.newType());
}

void PatternVar::eraseBindings(std::set<std::string> &from) const {
  from.erase(var);
}

void PatternVar::typecheck(
    std::shared_ptr<ff::sem::Type> t, ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) const {
  mgr.unify(typeCtx->lookup(var)->scheme->instantiate(mgr), t, loc);
}

void PatternConstr::typecheck(
    std::shared_ptr<ff::sem::Type> t, ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) const {
  auto constructor = typeCtx->lookup(constr);
  if (!constructor) {
    throw ff::TypeError(
        std::string("pattern using unknown constructor ") + constr, loc);
  }

  auto constructorType = constructor->scheme->instantiate(mgr);
  for (auto &param : params) {
    ff::sem::TypeArr *arr =
        dynamic_cast<ff::sem::TypeArr *>(constructorType.get());

    if (!arr)
      throw ff::TypeError("too many parameters in constructor pattern", loc);

    mgr.unify(typeCtx->lookup(param)->scheme->instantiate(mgr), arr->getLeft(),
              loc);
    constructorType = arr->getRight();
  }

  mgr.unify(t, constructorType, loc);
}

void PatternConstr::insertBindings(
    ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) const {
  for (auto &param : this->params) {
    typeCtx->bind(param, mgr.newType());
  }
}

void PatternConstr::eraseBindings(std::set<std::string> &from) const {
  for (auto &param : this->params) {
    from.erase(param);
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
  for (auto &param : params) {
    freeVariables.erase(param);
  }
}

void DefinitionDefn::insertTypes(ff::sem::TypeManager &) {
  typeContext->bind(name, fullType, visibility);
}

void DefinitionDefn::typecheck(ff::sem::TypeManager &mgr) {
  auto bodyType = body->typecheck(mgr);
  mgr.unify(returnType, bodyType, body->loc);
}

void DefinitionDefn::translate(GlobalScope &scope) {
  body->translate(scope);

  if (visibility == ff::sem::Visibility::Global)
    return;

  /* Only the names that live on the stack need capturing; a reference to a
   * global is reached by name at any depth. */
  for (auto &free : freeVariables) {
    auto variable = typeContext->lookup(free);
    if (variable && variable->visibility == ff::sem::Visibility::Local)
      capturedVariables.insert(free);
  }

  params.insert(params.begin(), capturedVariables.begin(),
                capturedVariables.end());
  scope.add(*this);
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
  generatedFunction =
      generator.createCustomFunction(mangledName, params.size());
}

void DefinitionDefn::generateLLVM(ff::cg::CodeGenerator &generator) {
  generator.getBuilder().SetInsertPoint(&generatedFunction->getEntryBlock());
  for (auto &instruction : instructions) {
    instruction->generate(generator, generatedFunction);
  }
  generator.getBuilder().CreateRetVoid();
}

void DefinitionData::insertTypes(
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) {
  this->typeContext = typeCtx;
  typeContext->bindType(
      name,
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeData(name, vars.size())),
      loc);
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
      throw ff::CompilerError(
          "type variable " + var + " used twice in data type definition", loc);

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
      std::shared_ptr<ff::sem::Type> type =
          (*it)->toType(varSet, *typeContext, loc);
      fullType =
          std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(type, fullType));
    }

    std::shared_ptr<ff::sem::TypeScheme> fullScheme(
        new ff::sem::TypeScheme(std::move(fullType)));

    fullScheme->forall.insert(fullScheme->forall.begin(), vars.begin(),
                              vars.end());

    typeContext->bind(constructor->name, fullScheme,
                      ff::sem::Visibility::Global);
  }
}

void generateConstructorLLVM(ff::cg::CodeGenerator &generator,
                             const std::string &name, int tag,
                             std::size_t arity) {
  auto newFunction = generator.createCustomFunction(name, arity);

  std::vector<std::unique_ptr<ff::ir::Instruction>> instructions;

  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Pack(tag, arity)));

  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(0)));

  generator.getBuilder().SetInsertPoint(&newFunction->getEntryBlock());
  for (auto &instruction : instructions) {
    instruction->generate(generator, newFunction);
  }

  generator.getBuilder().CreateRetVoid();
}

void DefinitionData::generateLLVM(ff::cg::CodeGenerator &generator) {
  for (auto &constructor : constructors) {
    generateConstructorLLVM(generator, constructor->name, constructor->tag,
                            constructor->types.size());
  }
}

// ############ Groups ############

void DefinitionGroup::findFree(ff::sem::TypeManager &mgr,
                               std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                               ff::sem::Visibility visibility,
                               std::set<std::string> &into) {
  this->typeContext = typeCtx;

  for (auto &defData : defsData) {
    defData.second->insertTypes(typeCtx);
  }
  for (auto &defData : defsData) {
    defData.second->insertConstructors();
  }

  ff::sem::FunctionGraph dependencyGraph;

  for (auto &defDefn : defsDefn) {
    defDefn.second->visibility = visibility;
    defDefn.second->findFree(mgr, typeCtx);
    dependencyGraph.addFunction(defDefn.second->name);

    /* A reference to a sibling orders this group; anything else belongs to
     * an enclosing scope and is passed further out. */
    for (auto &free : defDefn.second->freeVariables) {
      if (defsDefn.find(free) != defsDefn.end())
        dependencyGraph.addEdge(defDefn.second->name, free);
      else
        into.insert(free);
    }
  }

  groups = dependencyGraph.computeOrder();
}

void DefinitionGroup::typecheck(ff::sem::TypeManager &mgr) {
  for (auto it = groups.rbegin(); it != groups.rend(); it++) {
    auto &group = *it;
    for (auto &defDefnName : group->members) {
      defsDefn.find(defDefnName)->second->insertTypes(mgr);
    }

    for (auto &defDefnName : group->members) {
      defsDefn.find(defDefnName)->second->typecheck(mgr);
    }

    for (auto &defDefnName : group->members) {
      typeContext->generalize(defDefnName, group->members, mgr);
    }
  }
}

void DefinitionGroup::translate(GlobalScope &scope) {
  /* Every global claims its symbol before a single body is lifted. A global
   * is reached by name from anywhere, so it has to keep the name it was
   * written with, which the runtime calls by that name
   * and it is the lifted functions that give way and take a suffix. */
  for (auto &defDefn : defsDefn) {
    auto &definition = *defDefn.second;
    if (definition.visibility != ff::sem::Visibility::Global)
      continue;

    definition.mangledName = scope.mangle(definition.name);
    definition.typeContext->setMangledName(definition.name,
                                           definition.mangledName);
  }

  for (auto &defDefn : defsDefn) {
    defDefn.second->translate(scope);
  }
}

std::string GlobalScope::mangle(const std::string &name) {
  return mng->newMangledName(name);
}

void GlobalScope::add(DefinitionDefn &definition) {
  /* Two lifted definitions can easily share a name, every lambda is called
   * "lambda", and nested lets shadow each other, so each one takes the
   * next free variation of it. */
  definition.mangledName = mangle(definition.name);
  definitions.push_back(&definition);
}
