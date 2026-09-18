#include "ast.hpp"

#include "classes.hpp"
#include "context.hpp"
#include "enviroment.hpp"
#include "error.hpp"
#include "instructions.hpp"
#include "types.hpp"
#include <algorithm>
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

/* A reference to one of the functions the compiler generates. It names its
 * symbol outright, the way a lifted definition does. */
std::unique_ptr<Ast> evidenceGlobal(const std::string &symbol) {
  AstLid *global = new AstLid(symbol);
  global->lifted = true;
  return std::unique_ptr<Ast>(global);
}

/* The expression that builds a piece of evidence. A dictionary the
 * definition was handed is named; anything else is one of the generated
 * functions applied to evidence of its own. */
std::unique_ptr<Ast> evidenceAst(const ff::sem::EvidenceTerm &term) {
  if (term.parameter)
    return std::unique_ptr<Ast>(new AstLid(term.name));

  std::unique_ptr<Ast> application = evidenceGlobal(term.name);
  for (auto &argument : term.arguments)
    application = std::unique_ptr<Ast>(
        new AstApp(std::move(application), evidenceAst(*argument)));

  return application;
}

/* Push what a piece of evidence comes to. A dictionary the definition was
 * handed is a stack slot; anything else is one of the generated functions
 * applied to evidence of its own, which is an application like any other. */
void generateEvidence(const ff::sem::EvidenceTerm &term,
                      const std::shared_ptr<ff::ir::Enviroment> &env,
                      std::vector<std::unique_ptr<ff::ir::Instruction>> &into) {
  if (term.parameter) {
    into.push_back(std::unique_ptr<ff::ir::Instruction>(
        new ff::ir::Push(env->getOffset(term.name))));
    return;
  }

  std::shared_ptr<ff::ir::Enviroment> current = env;
  for (auto it = term.arguments.rbegin(); it != term.arguments.rend(); it++) {
    generateEvidence(**it, current, into);
    current = std::shared_ptr<ff::ir::Enviroment>(
        new ff::ir::EnviromentOffset(1, current));
  }

  into.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::PushGlobal(term.name)));

  for (std::size_t i = 0; i < term.arguments.size(); i++)
    into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
}

/* The type variables a signature writes, and the fresh variables they stand
 * for. Each definition gets its own set: two definitions that both write `a`
 * are not thereby talking about the same type. */
struct SignatureVars {
  std::set<std::string> written;
  std::map<std::string, std::shared_ptr<ff::sem::Type>> fresh;
};

SignatureVars signatureVars(ff::sem::TypeManager &mgr,
                            const DefinitionDefn &definition) {
  SignatureVars vars;

  for (auto &param : definition.params) {
    if (param->type)
      param->type->collectVariables(vars.written);
  }
  if (definition.returnAnnotation)
    definition.returnAnnotation->collectVariables(vars.written);
  for (auto &pred : definition.context)
    pred->collectVariables(vars.written);

  for (auto &name : vars.written)
    vars.fresh[name] = mgr.newType();

  return vars;
}

std::shared_ptr<ff::sem::Type>
resolveAnnotation(ff::sem::TypeManager &mgr, const ff::sem::ParsedType &parsed,
                  const SignatureVars &vars,
                  const ff::sem::TypeContext &typeCtx,
                  const yy::location &loc) {
  return mgr.substitute(vars.fresh, parsed.toType(vars.written, typeCtx, loc));
}

/* Report an operand whose type is already known to be one the operator
 * cannot take. Unification would notice it too, but only as two types that
 * did not fit, without saying which side of the operator went wrong. */
void checkOperand(ff::sem::TypeManager &mgr, const char *side, binop op,
                  const std::shared_ptr<ff::sem::Type> &expected,
                  const Ast &operand,
                  const std::shared_ptr<ff::sem::Type> &actual) {
  ff::sem::TypeVar *var;
  auto resolved = mgr.resolve(actual, var);

  /* A type still open here may yet turn out to fit. */
  if (var)
    return;

  auto *expectedApp = dynamic_cast<ff::sem::TypeApp *>(expected.get());
  auto *resolvedApp = dynamic_cast<ff::sem::TypeApp *>(resolved.get());
  if (!expectedApp ||
      (resolvedApp && resolvedApp->constructor == expectedApp->constructor))
    return;

  std::ostringstream errorStream;
  errorStream << "the " << side << " operand of " << opName(op) << " is not ";
  expectedApp->constructor->print(mgr, errorStream);
  errorStream << ", its type is ";
  resolved->print(mgr, errorStream);

  throw ff::TypeError(errorStream.str(), operand.loc);
}

/* One side of a composition is resolved. A side already known not to be a
 * function is worth naming outright; unifying it would only report a
 * mismatch against an arrow the program never wrote. */
std::shared_ptr<ff::sem::Type>
resolveComposed(ff::sem::TypeManager &mgr, const char *side, const Ast &operand,
                const std::shared_ptr<ff::sem::Type> &type) {
  ff::sem::TypeVar *var;
  auto resolved = mgr.resolve(type, var);

  /* A type still open here may yet turn out to be a function. */
  if (var || dynamic_cast<ff::sem::TypeArr *>(resolved.get()))
    return resolved;

  std::ostringstream errorStream;
  errorStream << "the " << side << " side of . is not a function, its type is ";
  resolved->print(mgr, errorStream);

  throw ff::TypeError(errorStream.str(), operand.loc);
}

/* An ambiguous variable is reported with what constrains it, since the
 * constraint is what the reader has to remove or pin down. */
std::string ambiguityMessage(ff::sem::TypeManager &mgr, const std::string &var,
                             const std::vector<ff::sem::Wanted> &retained) {
  ff::sem::TypeNamer namer;
  std::ostringstream errorStream;

  errorStream << "the type variable " << namer.nameOf(var)
              << " is ambiguous: nothing that uses this can say what it is, "
                 "and it is constrained by";

  for (auto &one : retained) {
    std::set<std::string> predVars;
    mgr.findFree(one.pred.type, predVars);
    if (predVars.find(var) == predVars.end())
      continue;

    errorStream << " ";
    ff::sem::printReadable(mgr, one.pred, namer, errorStream);
  }

  return errorStream.str();
}

std::string notEntailedMessage(ff::sem::TypeManager &mgr,
                               const ff::sem::Pred &pred,
                               const std::vector<ff::sem::Pred> &declared) {
  ff::sem::TypeNamer namer;
  std::ostringstream errorStream;

  errorStream << "the body needs ";
  ff::sem::printReadable(mgr, pred, namer, errorStream);
  errorStream << ", which the declared context";

  for (auto &one : declared) {
    errorStream << " ";
    ff::sem::printReadable(mgr, one, namer, errorStream);
  }
  errorStream << " does not provide";

  return errorStream.str();
}

/* What the right side hands back must be what the left side takes. Both
 * being known and different is the mistake worth reporting, rather than the
 * two whole function types unification would hold against each other. */
void checkComposedTypes(ff::sem::TypeManager &mgr, const Ast &compose,
                        const std::shared_ptr<ff::sem::Type> &taken,
                        const std::shared_ptr<ff::sem::Type> &handedBack) {
  ff::sem::TypeVar *takenVar;
  ff::sem::TypeVar *handedBackVar;
  auto resolvedTaken = mgr.resolve(taken, takenVar);
  auto resolvedHandedBack = mgr.resolve(handedBack, handedBackVar);
  if (takenVar || handedBackVar)
    return;

  auto *takenApp = dynamic_cast<ff::sem::TypeApp *>(resolvedTaken.get());
  auto *handedBackApp =
      dynamic_cast<ff::sem::TypeApp *>(resolvedHandedBack.get());
  if (!takenApp || !handedBackApp ||
      takenApp->constructor == handedBackApp->constructor)
    return;

  std::ostringstream errorStream;
  errorStream << "the left side of . takes ";
  resolvedTaken->print(mgr, errorStream);
  errorStream << ", but the right side of . hands back ";
  resolvedHandedBack->print(mgr, errorStream);

  throw ff::TypeError(errorStream.str(), compose.loc);
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
  /* Whatever the name holds under is taken on here, and a slot kept for
   * each so that solving can say what to apply the name to. A use from
   * inside the name's own group finds nothing to hold under yet, and is
   * told what it passes along once the group is generalized. */
  variable->uses.push_back(&evidence);
  return variable->scheme->instantiate(mgr, loc, &evidence);
}

void AstLid::translate(GlobalScope &) {}

void AstLid::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  /* The dictionaries the name holds under go down first, deepest last, so
   * that pushing the name itself leaves the whole application to be built by
   * one MkApp each. */
  std::shared_ptr<ff::ir::Enviroment> current = env;
  for (auto it = evidence.rbegin(); it != evidence.rend(); it++) {
    /* Every slot is filled before code is generated. */
    assert((*it)->term != nullptr);
    generateEvidence(*(*it)->term, current, into);
    current = std::shared_ptr<ff::ir::Enviroment>(
        new ff::ir::EnviromentOffset(1, current));
  }

  /* A lifted reference names its global outright; it was created after
   * typechecking and so has no scope to resolve the name against. */
  if (lifted) {
    into.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::PushGlobal(id)));
  } else {
    into.push_back(std::unique_ptr<ff::ir::Instruction>(
        current->hasVariable(id)
            ? (ff::ir::Instruction *)new ff::ir::Push(current->getOffset(id))
            : (ff::ir::Instruction *)new ff::ir::PushGlobal(
                  this->typeContext->getMangledName(id))));
  }

  for (std::size_t i = 0; i < evidence.size(); i++)
    into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
}

void AstLid::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "LID: " << id << std::endl;

  for (auto &slot : evidence) {
    printIndent(indent + 1, to);
    to << "DICT: ";
    if (slot->term)
      slot->term->print(to);
    else
      to << "<unsolved>";
    to << std::endl;
  }
}

std::shared_ptr<ff::sem::Type> AstUid::typecheck(ff::sem::TypeManager &mgr) {
  auto constructor = typeContext->lookup(id);
  if (!constructor)
    throw ff::TypeError("unknown constructor " + id, loc);
  return constructor->scheme->instantiate(mgr, loc);
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
  to << "UID: " << id << std::endl;
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

  auto ftype = opVariable->scheme->instantiate(mgr, loc);

  /* Every operator takes two operands of a type it fixes itself, so the
   * types it expects can be read straight off it and blamed one at a time. */
  if (auto *firstArrow = dynamic_cast<ff::sem::TypeArr *>(ftype.get())) {
    checkOperand(mgr, "left", op, firstArrow->getLeft(), *left, ltype);
    if (auto *secondArrow =
            dynamic_cast<ff::sem::TypeArr *>(firstArrow->getRight().get()))
      checkOperand(mgr, "right", op, secondArrow->getLeft(), *right, rtype);
  }

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
  this->function->generate(
      std::shared_ptr<ff::ir::Enviroment>(new ff::ir::EnviromentOffset(1, env)),
      into);

  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
}

void AstPipe::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "PIPE:" << std::endl;
  value->print(indent + 1, to);
  function->print(indent + 1, to);
}

std::shared_ptr<ff::sem::Type>
AstCompose::typecheck(ff::sem::TypeManager &mgr) {
  auto leftType = left->typecheck(mgr);
  auto rightType = right->typecheck(mgr);

  auto resolvedLeft = resolveComposed(mgr, "left", *left, leftType);
  auto resolvedRight = resolveComposed(mgr, "right", *right, rightType);

  auto *leftArrow = dynamic_cast<ff::sem::TypeArr *>(resolvedLeft.get());
  auto *rightArrow = dynamic_cast<ff::sem::TypeArr *>(resolvedRight.get());
  if (leftArrow && rightArrow)
    checkComposedTypes(mgr, *this, leftArrow->getLeft(),
                       rightArrow->getRight());

  auto composeVariable = typeContext->lookup(ff::sem::composeName);
  /* The compiler binds the composition operator before anything is read. */
  assert(composeVariable != nullptr);

  auto composeType = composeVariable->scheme->instantiate(mgr, loc);

  auto returnType = mgr.newType();
  auto arrowOne = std::shared_ptr<ff::sem::Type>(
      new ff::sem::TypeArr(rightType, returnType));
  auto arrowTwo =
      std::shared_ptr<ff::sem::Type>(new ff::sem::TypeArr(leftType, arrowOne));

  mgr.unify(composeType, arrowTwo, loc);
  return returnType;
}

void AstCompose::findFree(ff::sem::TypeManager &mgr,
                          std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                          std::set<std::string> &into) {
  this->typeContext = typeCtx;
  left->findFree(mgr, typeCtx, into);
  right->findFree(mgr, typeCtx, into);
}

void AstCompose::translate(GlobalScope &scope) {
  left->translate(scope);
  right->translate(scope);
}

void AstCompose::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  /* The same graph an application of the composition supercombinator to
   * both sides would build. */
  this->right->generate(env, into);
  this->left->generate(
      std::shared_ptr<ff::ir::Enviroment>(new ff::ir::EnviromentOffset(1, env)),
      into);

  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::PushGlobal(
      this->typeContext->getMangledName(ff::sem::composeName))));
  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::MkApp()));
}

void AstCompose::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "COMPOSE:" << std::endl;
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

std::shared_ptr<ff::sem::Type> AstIf::typecheck(ff::sem::TypeManager &mgr) {
  auto boolType = typeContext->lookupType(ff::sem::boolTypeName);
  auto *boolData = dynamic_cast<ff::sem::TypeData *>(boolType.get());

  if (!boolData)
    throw ff::TypeError(std::string("an if expression needs the ") +
                            ff::sem::boolTypeName +
                            " data type, which is not defined",
                        loc);

  auto trueIt = boolData->constructors.find(ff::sem::boolTrueName);
  auto falseIt = boolData->constructors.find(ff::sem::boolFalseName);
  if (trueIt == boolData->constructors.end() ||
      falseIt == boolData->constructors.end())
    throw ff::TypeError(std::string("an if expression needs the ") +
                            ff::sem::boolTypeName + " type to have the " +
                            ff::sem::boolTrueName + " and " +
                            ff::sem::boolFalseName + " constructors",
                        loc);

  this->trueTag = trueIt->second.tag;
  this->falseTag = falseIt->second.tag;

  auto conditionType = condition->typecheck(mgr);

  /* A condition already known to be something else is worth reporting rigth
   * away */
  ff::sem::TypeVar *var;
  auto resolved = mgr.resolve(conditionType, var);
  auto *resolvedApp = dynamic_cast<ff::sem::TypeApp *>(resolved.get());
  if (!var && (!resolvedApp || resolvedApp->constructor.get() != boolData)) {
    std::ostringstream errorStream;
    errorStream << "the condition of an if expression is not a "
                << ff::sem::boolTypeName << ", its type is ";
    resolved->print(mgr, errorStream);

    throw ff::TypeError(errorStream.str(), condition->loc);
  }

  mgr.unify(std::shared_ptr<ff::sem::Type>(new ff::sem::TypeApp(boolType)),
            conditionType, condition->loc);

  auto thenType = thenBranch->typecheck(mgr);
  auto elseType = elseBranch->typecheck(mgr);
  mgr.unify(thenType, elseType, elseBranch->loc);

  return thenType;
}

void AstIf::findFree(ff::sem::TypeManager &mgr,
                     std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                     std::set<std::string> &into) {
  this->typeContext = typeCtx;
  condition->findFree(mgr, typeCtx, into);
  thenBranch->findFree(mgr, typeCtx, into);
  elseBranch->findFree(mgr, typeCtx, into);
}

void AstIf::translate(GlobalScope &scope) {
  condition->translate(scope);
  thenBranch->translate(scope);
  elseBranch->translate(scope);
}

void AstIf::generate(
    const std::shared_ptr<ff::ir::Enviroment> &env,
    std::vector<std::unique_ptr<ff::ir::Instruction>> &into) const {
  condition->generate(env, into);
  into.push_back(std::unique_ptr<ff::ir::Instruction>(new ff::ir::Eval()));

  ff::ir::Jump *jumpInstruction = new ff::ir::Jump();
  into.push_back(std::unique_ptr<ff::ir::Instruction>(jumpInstruction));

  const std::pair<int, const Ast *> arms[] = {{trueTag, thenBranch.get()},
                                              {falseTag, elseBranch.get()}};

  for (auto &[tag, arm] : arms) {
    std::vector<std::unique_ptr<ff::ir::Instruction>> branchInstructions;

    /* Neither branch binds anything, so the split only drops the condition
     * and leaves the branch result alone on the stack. */
    branchInstructions.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::Split(0)));
    arm->generate(env, branchInstructions);

    jumpInstruction->tagMappings[tag] = jumpInstruction->branches.size();
    jumpInstruction->branches.push_back(std::move(branchInstructions));
  }
}

void AstIf::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "IF:" << std::endl;
  condition->print(indent + 1, to);
  printIndent(indent, to);
  to << "THEN:" << std::endl;
  thenBranch->print(indent + 1, to);
  printIndent(indent, to);
  to << "ELSE:" << std::endl;
  elseBranch->print(indent + 1, to);
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

  definitions->insertDataTypes(this->typeContext);
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
      to << " " << param->name;
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
  mgr.unify(typeCtx->lookup(var)->scheme->instantiate(mgr, loc), t, loc);
}

void PatternConstr::typecheck(
    std::shared_ptr<ff::sem::Type> t, ff::sem::TypeManager &mgr,
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) const {
  auto constructor = typeCtx->lookup(constr);
  if (!constructor) {
    throw ff::TypeError(
        std::string("pattern using unknown constructor ") + constr, loc);
  }

  auto constructorType = constructor->scheme->instantiate(mgr, loc);
  for (auto &param : params) {
    ff::sem::TypeArr *arr =
        dynamic_cast<ff::sem::TypeArr *>(constructorType.get());

    if (!arr)
      throw ff::TypeError("too many parameters in constructor pattern", loc);

    mgr.unify(typeCtx->lookup(param)->scheme->instantiate(mgr, loc), arr->getLeft(),
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

DefinitionDefn::DefinitionDefn(std::string n, const std::vector<std::string> &p,
                               std::unique_ptr<Ast> b, yy::location lc)
    : name(std::move(n)), body(std::move(b)),
      visibility(ff::sem::Visibility::Global), mangledName(name),
      loc(std::move(lc)) {
  for (auto &param : p)
    params.push_back(std::unique_ptr<Param>(new Param(param)));
}

void DefinitionDefn::findFree(ff::sem::TypeManager &mgr,
                              std::shared_ptr<ff::sem::TypeContext> &typeCtx) {
  this->typeContext = typeCtx;

  varContext = ff::sem::typeScope(typeCtx);

  /* Whatever the signature declared is used as it stands; the rest is left
   * open for inference to fill in, exactly as before. */
  auto signature = signatureVars(mgr, *this);

  returnType = returnAnnotation
                   ? resolveAnnotation(mgr, *returnAnnotation, signature,
                                       *typeCtx, returnAnnotationLoc)
                   : mgr.newType();
  fullType = returnType;

  for (auto it = params.rbegin(); it != params.rend(); it++) {
    auto &param = **it;
    auto paramType = param.type ? resolveAnnotation(mgr, *param.type, signature,
                                                    *typeCtx, param.loc)
                                : mgr.newType();
    fullType = std::shared_ptr<ff::sem::Type>(
        new ff::sem::TypeArr(paramType, fullType));
    varContext->bind(param.name, paramType);
  }

  for (auto &pred : context) {
    const ff::sem::ClassEnv *classEnv = mgr.getClassEnv();
    /* Inference never runs before the class environment is built. */
    assert(classEnv != nullptr);

    if (!classEnv->lookup(pred->className))
      throw ff::TypeError("unknown class " + pred->className +
                              " in the context of " + name,
                          pred->loc);

    if (pred->arguments.size() != 1)
      throw ff::TypeError("the constraint on " + name + " applies " +
                              pred->className + " to more than one type",
                          pred->loc);

    declaredContext.push_back(ff::sem::Pred(
        pred->className,
        resolveAnnotation(mgr, *pred->arguments.front(), signature, *typeCtx,
                          pred->loc)));
  }

  body->findFree(mgr, varContext, freeVariables);
  for (auto &param : params) {
    freeVariables.erase(param->name);
  }
}

void DefinitionDefn::insertTypes(ff::sem::TypeManager &) {
  typeContext->bind(name, fullType, visibility);
}

void DefinitionDefn::typecheck(ff::sem::TypeManager &mgr) {
  auto bodyType = body->typecheck(mgr);

  if (returnAnnotation && returnDescription.empty())
    returnDescription =
        "the body of " + name + " does not have its declared return type";

  /* When the result was fixed by something other than the body -- a return
   * type the definition declared, or the class an instance method belongs to
   * -- the two whole types are worth showing rather than whichever pair of
   * pieces deep inside them happened not to fit. */
  try {
    mgr.unify(returnType, bodyType, body->loc);
  } catch (const ff::UnificationError &) {
    if (returnDescription.empty())
      throw;

    throw ff::UnificationError(returnType, bodyType, body->loc,
                               returnDescription);
  }
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

  std::vector<std::unique_ptr<Param>> withCaptures;
  for (auto &captured : capturedVariables)
    withCaptures.push_back(std::unique_ptr<Param>(new Param(captured)));

  withCaptures.insert(withCaptures.end(),
                      std::make_move_iterator(params.begin()),
                      std::make_move_iterator(params.end()));
  params = std::move(withCaptures);

  scope.add(*this);
}

void DefinitionDefn::compile() {
  auto newEnv = std::shared_ptr<ff::ir::Enviroment>(
      new ff::ir::EnviromentOffset(0, nullptr));

  for (auto it = params.rbegin(); it != params.rend(); it++) {
    newEnv = std::shared_ptr<ff::ir::Enviroment>(
        new ff::ir::EnviromentVar((*it)->name, newEnv));
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

const DefinitionDefn *findDuplicateMethod(
    const std::vector<std::unique_ptr<DefinitionDefn>> &methods) {
  std::set<std::string> seen;
  for (auto &method : methods) {
    if (!seen.insert(method->name).second)
      return method.get();
  }
  return nullptr;
}

// ############ Groups ############

/* Bind what the group's data declarations name, ahead of anything that could
 * mention them. Kept apart from findFree because the class environment is
 * built between the two: an instance head names a type, and so needs the
 * types to be in scope, but nothing about it depends on inference having
 * started. */
void DefinitionGroup::insertDataTypes(
    std::shared_ptr<ff::sem::TypeContext> &typeCtx) {
  for (auto &defData : defsData) {
    defData.second->insertTypes(typeCtx);
  }
  for (auto &defData : defsData) {
    defData.second->insertConstructors();
  }
}

void DefinitionGroup::findFree(ff::sem::TypeManager &mgr,
                               std::shared_ptr<ff::sem::TypeContext> &typeCtx,
                               ff::sem::Visibility visibility,
                               std::set<std::string> &into) {
  this->typeContext = typeCtx;

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

    /* Everything wanted from here on belongs to this group, and is answered
     * for once its bodies have been checked. */
    std::size_t mark = mgr.wantedMark();

    for (auto &defDefnName : group->members) {
      defsDefn.find(defDefnName)->second->insertTypes(mgr);
    }

    for (auto &defDefnName : group->members) {
      defsDefn.find(defDefnName)->second->typecheck(mgr);
    }

    generalizeGroup(mgr, *group, mark);
  }
}

void DefinitionGroup::generalizeGroup(ff::sem::TypeManager &mgr,
                                      const ff::sem::Group &group,
                                      std::size_t mark) {
  const ff::sem::ClassEnv *classEnv = mgr.getClassEnv();
  /* Inference never runs before the class environment is built. */
  assert(classEnv != nullptr);

  /* A context the program wrote is what the definition promised, and is
   * checked against what its body turned out to need. Only a definition that
   * stands on its own may write one: the members of a group are inferred
   * together, so a context on one of them would be a claim about all. */
  std::vector<ff::sem::Pred> declared;
  bool anyDeclared = false;
  for (auto &name : group.members) {
    auto &definition = *defsDefn.find(name)->second;
    if (definition.declaredContext.empty())
      continue;

    anyDeclared = true;
    if (group.members.size() > 1)
      throw ff::TypeError(
          "the definition " + name +
              " writes a context, but is mutually recursive with another "
              "definition; leave the context off and it will be inferred",
          definition.loc);

    declared.insert(declared.end(), definition.declaredContext.begin(),
                    definition.declaredContext.end());
  }

  /* The constraints as the bodies wanted them, kept because each is what a
   * use is waiting on the answer to; and the same set reduced, which is what
   * the group holds under. */
  auto original = mgr.takeWantedFrom(mark);
  auto wanted = classEnv->reduce(mgr, original, declared);

  /* Type variables the scope around the group already talks about. One of
   * those is not this group's to quantify, and a constraint about nothing
   * else is not this group's to answer for either. */
  std::set<std::string> envVars;
  typeContext->findFree(mgr, group.members, envVars);

  std::set<std::string> groupVars;
  for (auto &name : group.members)
    mgr.findFree(typeContext->lookup(name)->scheme->monotype, groupVars);

  /* Split: a constraint about nothing the group is free to choose belongs to
   * the scope around it, and the rest is what the group holds under. */
  std::vector<ff::sem::Wanted> deferred;
  std::vector<ff::sem::Wanted> retained;
  for (auto &one : wanted) {
    std::set<std::string> predVars;
    mgr.findFree(one.pred.type, predVars);

    bool allFixed = true;
    for (auto &var : predVars) {
      if (envVars.find(var) == envVars.end()) {
        allFixed = false;
        break;
      }
    }

    (allFixed ? deferred : retained).push_back(one);
  }

  /* The monomorphism restriction. A definition that takes no arguments and
   * promises nothing is a value, not a function, and a value is computed
   * once: generalizing what constrains it would have it computed afresh, at
   * a different type, at every use. Its constrained variables are therefore
   * left alone to be settled by defaulting or by whatever uses it. */
  std::set<std::string> notQuantified = envVars;
  for (auto &name : group.members) {
    auto &definition = *defsDefn.find(name)->second;
    if (!definition.params.empty() || !definition.declaredContext.empty())
      continue;

    for (auto &one : retained)
      mgr.findFree(one.pred.type, notQuantified);
    deferred.insert(deferred.end(), retained.begin(), retained.end());
    retained.clear();
    break;
  }

  /* A constraint about a variable none of the group's types mention can
   * never be settled by a caller, since a caller has nothing to settle it
   * with. Defaulting is the one way out. */
  std::set<std::string> ambiguous;
  for (auto &one : retained) {
    std::set<std::string> predVars;
    mgr.findFree(one.pred.type, predVars);
    for (auto &var : predVars) {
      if (groupVars.find(var) == groupVars.end())
        ambiguous.insert(var);
    }
  }

  if (!ambiguous.empty()) {
    std::vector<ff::sem::Pred> preds;
    for (auto &one : retained)
      preds.push_back(one.pred);

    for (auto &var : ambiguous) {
      if (classEnv->defaultVariable(mgr, var, preds))
        continue;

      throw ff::TypeError(ambiguityMessage(mgr, var, retained),
                          retained.front().loc);
    }

    /* Defaulting settled some variables, so the constraints about them are
     * about a known type now and reduce away. */
    retained = classEnv->reduce(mgr, std::move(retained));
  }

  if (anyDeclared) {
    for (auto &one : retained) {
      if (classEnv->entail(mgr, declared, one.pred))
        continue;

      throw ff::TypeError(notEntailedMessage(mgr, one.pred, declared), one.loc);
    }
  }

  std::vector<ff::sem::Pred> schemeContext;
  if (anyDeclared) {
    schemeContext = declared;
  } else {
    for (auto &one : retained)
      schemeContext.push_back(one.pred);
  }

  /* The canonical order the dictionaries are taken in: by class name, then
   * by the type constrained. Fixed here and nowhere else, so that a use and
   * the definition it applies itself to agree without either having to look
   * at the other. */
  std::sort(schemeContext.begin(), schemeContext.end(),
            [&mgr](const ff::sem::Pred &left, const ff::sem::Pred &right) {
              if (left.className != right.className)
                return left.className < right.className;
              return ff::sem::structuralKey(mgr, left.type) <
                     ff::sem::structuralKey(mgr, right.type);
            });

  /* One dictionary parameter per constraint. They are what the group has in
   * hand while its uses are solved, and what it takes as arguments. */
  std::vector<ff::sem::Given> givens;
  std::vector<std::string> dictionaryParams;
  for (std::size_t i = 0; i < schemeContext.size(); i++) {
    auto paramName = ff::sem::dictionaryParamName(schemeContext[i].className, i);
    dictionaryParams.push_back(paramName);
    givens.push_back(ff::sem::Given(
        schemeContext[i], ff::sem::EvidenceTerm::ofParameter(paramName)));
  }

  for (auto &name : group.members) {
    auto &definition = *defsDefn.find(name)->second;
    auto &scheme = *typeContext->lookup(name)->scheme;
    /* A binding is only ever reached once by its own group. */
    assert(scheme.forall.empty() && scheme.context.empty());

    scheme.context = schemeContext;

    std::set<std::string> ownVars;
    mgr.findFree(scheme.monotype, ownVars);
    for (auto &pred : schemeContext)
      mgr.findFree(pred.type, ownVars);

    for (auto &var : ownVars) {
      if (notQuantified.find(var) == notQuantified.end())
        scheme.forall.push_back(var);
    }

    definition.dictionaryParams = dictionaryParams;

    /* A use from inside the group passes on the very dictionaries the group
     * was handed: it is the same definition, at the same type. */
    for (auto *use : typeContext->lookup(name)->uses) {
      if (!use->empty())
        continue;

      for (auto &given : givens) {
        std::shared_ptr<ff::sem::EvidenceSlot> slot(new ff::sem::EvidenceSlot());
        slot->term = given.evidence;
        use->push_back(std::move(slot));
      }
    }

    /* Ahead of the parameters the definition wrote, and bound where anything
     * nested inside it will find them, so that lambda lifting captures them
     * the way it captures any other local. */
    std::vector<std::unique_ptr<Param>> withDictionaries;
    for (auto &paramName : dictionaryParams) {
      withDictionaries.push_back(
          std::unique_ptr<Param>(new Param(paramName, nullptr, definition.loc)));
      definition.varContext->bind(paramName, mgr.newType(),
                                  ff::sem::Visibility::Local);
    }

    withDictionaries.insert(withDictionaries.end(),
                            std::make_move_iterator(definition.params.begin()),
                            std::make_move_iterator(definition.params.end()));
    definition.params = std::move(withDictionaries);
  }

  /* Every use that took a constraint on is told how it is answered. What
   * this group cannot answer is handed outward, still attached to the use,
   * so the scope that knows what its variables are answers it instead. */
  for (auto &one : original) {
    auto term = classEnv->solve(mgr, givens, one.pred);
    if (!term) {
      mgr.want(one);
      continue;
    }

    if (one.slot)
      one.slot->term = std::move(term);
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

// ############ Classes and instances ############

/* A class becomes a data type of one constructor, whose fields are the
 * dictionaries of its superclasses followed by its methods, and one selector
 * for each of those fields. An instance becomes a function from the
 * dictionaries it holds under to the dictionary it builds.
 *
 * The fields go in the order the constructor takes its arguments, which
 * Split then hands back at the same offsets, so a selector is a match on the
 * one constructor and nothing more. */

void DefinitionClass::generateLLVM(ff::cg::CodeGenerator &generator) {
  std::size_t arity = dictionaryArity();

  generateConstructorLLVM(
      generator, ff::sem::dictionaryConstructorName(getName()), 0, arity);

  auto selector = [&](const std::string &symbol, std::size_t field) {
    auto function = generator.createCustomFunction(symbol, 1);

    std::vector<std::unique_ptr<ff::ir::Instruction>> instructions;
    instructions.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::Push(0)));
    /* The dictionary reaches this as a thunk the first time round: an
     * instance builds it lazily, like anything else. */
    instructions.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::Eval()));
    instructions.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::Split(arity)));
    instructions.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::Push(field)));
    instructions.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::Slide(arity)));
    instructions.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(1)));
    instructions.push_back(
        std::unique_ptr<ff::ir::Instruction>(new ff::ir::Pop(1)));

    generator.getBuilder().SetInsertPoint(&function->getEntryBlock());
    for (auto &instruction : instructions)
      instruction->generate(generator, function);
    generator.getBuilder().CreateRetVoid();
  };

  std::size_t field = 0;
  for (auto &super : supers)
    selector(ff::sem::superSelectorName(getName(), super->className), field++);
  for (auto &method : methods)
    selector(ff::sem::methodSelectorName(getName(), method->name), field++);
}

void DefinitionInstance::compile() {
  std::size_t arity = dictionaryParams.size();

  auto paramEnv = std::shared_ptr<ff::ir::Enviroment>(
      new ff::ir::EnviromentOffset(0, nullptr));
  for (auto it = dictionaryParams.rbegin(); it != dictionaryParams.rend(); it++)
    paramEnv = std::shared_ptr<ff::ir::Enviroment>(
        new ff::ir::EnviromentVar(*it, paramEnv));

  /* A placeholder for the dictionary itself, so that a default method can be
   * handed the very dictionary it is a field of. Nothing forces it while it
   * is being built: every field is an unevaluated graph, so the knot is tied
   * by the Update below rather than by anything looking at it. */
  const std::string self = std::string("d") + ff::sem::generatedMarker + "self";

  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Alloc(1)));

  auto env = std::shared_ptr<ff::ir::Enviroment>(
      new ff::ir::EnviromentVar(self, paramEnv));

  std::unique_ptr<Ast> dictionary = evidenceGlobal(
      ff::sem::dictionaryConstructorName(head->className));

  for (auto &field : dictionaryFields) {
    std::unique_ptr<Ast> value;

    if (field.evidence) {
      value = evidenceAst(*field.evidence);
    } else if (field.method) {
      value = partialApplication(*field.method);
    } else {
      /* The class's default, handed the dictionary it is a field of, so that
       * it may call any other method of the same class. */
      value = std::unique_ptr<Ast>(
          new AstApp(evidenceGlobal(field.defaultSymbol),
                     std::unique_ptr<Ast>(new AstLid(self))));
    }

    dictionary = std::unique_ptr<Ast>(
        new AstApp(std::move(dictionary), std::move(value)));
  }

  dictionary->generate(env, instructions);

  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(0)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Update(arity)));
  instructions.push_back(
      std::unique_ptr<ff::ir::Instruction>(new ff::ir::Pop(arity)));
}

void DefinitionInstance::declareLLVM(ff::cg::CodeGenerator &generator) {
  generatedFunction =
      generator.createCustomFunction(mangledName, dictionaryParams.size());
}

void DefinitionInstance::generateLLVM(ff::cg::CodeGenerator &generator) {
  generator.getBuilder().SetInsertPoint(&generatedFunction->getEntryBlock());
  for (auto &instruction : instructions)
    instruction->generate(generator, generatedFunction);
  generator.getBuilder().CreateRetVoid();
}

// ############ Evidence ############

/* Dictionaries are not part of the tree: they are terms hung off the uses
 * that need them, since nothing typechecks, lifts or pattern matches on one.
 * What still has to be walked is which of them a definition reaches for
 * without having been handed it, because that is what lambda lifting must
 * capture. */

void AstInt::findEvidence(std::set<std::string> &) {}

void AstLid::findEvidence(std::set<std::string> &into) {
  for (auto &slot : evidence) {
    /* Every slot is filled before this runs. */
    assert(slot->term != nullptr);
    slot->term->collectParameters(into);
  }
}

void AstUid::findEvidence(std::set<std::string> &) {}

void AstList::findEvidence(std::set<std::string> &into) {
  for (auto &item : items)
    item->findEvidence(into);
}

void AstBinop::findEvidence(std::set<std::string> &into) {
  left->findEvidence(into);
  right->findEvidence(into);
}

void AstApp::findEvidence(std::set<std::string> &into) {
  left->findEvidence(into);
  right->findEvidence(into);
}

void AstPipe::findEvidence(std::set<std::string> &into) {
  value->findEvidence(into);
  function->findEvidence(into);
}

void AstCompose::findEvidence(std::set<std::string> &into) {
  left->findEvidence(into);
  right->findEvidence(into);
}

void AstCase::findEvidence(std::set<std::string> &into) {
  of->findEvidence(into);
  for (auto &branch : branches)
    branch->expr->findEvidence(into);
}

void AstIf::findEvidence(std::set<std::string> &into) {
  condition->findEvidence(into);
  thenBranch->findEvidence(into);
  elseBranch->findEvidence(into);
}

/* A lambda takes no dictionaries of its own, so whatever its body reaches
 * for it has to capture, and so does whatever encloses it. */
void AstLambda::findEvidence(std::set<std::string> &into) {
  std::set<std::string> used;
  body->findEvidence(used);

  freeVariables.insert(used.begin(), used.end());
  into.insert(used.begin(), used.end());
}

void AstLet::findEvidence(std::set<std::string> &into) {
  definitions->findEvidence(into);
  in->findEvidence(into);
}

void DefinitionDefn::findEvidence(std::set<std::string> &into) {
  std::set<std::string> used;
  body->findEvidence(used);

  /* What it was handed, it does not have to reach for. */
  for (auto &param : dictionaryParams)
    used.erase(param);

  freeVariables.insert(used.begin(), used.end());
  into.insert(used.begin(), used.end());
}

void DefinitionGroup::findEvidence(std::set<std::string> &into) {
  for (auto &pair : defsDefn)
    pair.second->findEvidence(into);
}

// ############ Source printing ############

/* Writing the tree back out as source is what makes a parse test able to say
 * that two programs are the same program. Everything compound is
 * parenthesized: parentheses build no node of their own, so what is printed
 * parses to the tree it was printed from, and printing that again is the
 * same text. */

namespace {

void printParamsSource(const std::vector<std::unique_ptr<Param>> &params,
                       std::ostream &to) {
  for (auto &param : params) {
    to << " ";
    if (!param->type) {
      to << param->name;
      continue;
    }

    to << "(" << param->name << ": ";
    param->type->printSource(to);
    to << ")";
  }
}

} // namespace

void AstInt::printSource(std::ostream &to) const { to << value; }

void AstLid::printSource(std::ostream &to) const { to << id; }

void AstUid::printSource(std::ostream &to) const { to << id; }

void AstList::printSource(std::ostream &to) const {
  to << "[";
  for (auto it = items.begin(); it != items.end(); it++) {
    if (it != items.begin())
      to << ", ";
    (*it)->printSource(to);
  }
  to << "]";
}

void AstBinop::printSource(std::ostream &to) const {
  to << "(";
  left->printSource(to);
  to << " " << opName(op) << " ";
  right->printSource(to);
  to << ")";
}

void AstApp::printSource(std::ostream &to) const {
  to << "(";
  left->printSource(to);
  to << " ";
  right->printSource(to);
  to << ")";
}

void AstPipe::printSource(std::ostream &to) const {
  to << "(";
  value->printSource(to);
  to << " |> ";
  function->printSource(to);
  to << ")";
}

void AstCompose::printSource(std::ostream &to) const {
  to << "(";
  left->printSource(to);
  to << " . ";
  right->printSource(to);
  to << ")";
}

void AstCase::printSource(std::ostream &to) const {
  to << "(match ";
  of->printSource(to);
  to << " with { ";
  for (auto &branch : branches) {
    branch->pattern->printSource(to);
    to << " -> { ";
    branch->expr->printSource(to);
    to << " } ";
  }
  to << "})";
}

void AstIf::printSource(std::ostream &to) const {
  to << "(if ";
  condition->printSource(to);
  to << " { ";
  thenBranch->printSource(to);
  to << " } else { ";
  elseBranch->printSource(to);
  to << " })";
}

void AstLambda::printSource(std::ostream &to) const {
  to << "(\\";
  for (auto it = params.begin(); it != params.end(); it++) {
    if (it != params.begin())
      to << " ";
    to << *it;
  }
  to << " -> { ";
  body->printSource(to);
  to << " })";
}

void AstLet::printSource(std::ostream &to) const {
  to << "(let { ";
  for (auto &pair : definitions->defsDefn) {
    pair.second->printSource(to, 0);
    to << " ";
  }
  to << "} in { ";
  in->printSource(to);
  to << " })";
}

void PatternVar::printSource(std::ostream &to) const { print(to); }

void PatternConstr::printSource(std::ostream &to) const { print(to); }

void DefinitionDefn::printSource(std::ostream &to, int indent) const {
  printIndent(indent, to);
  to << "fun ";
  ff::sem::printContextSource(context, to);
  to << name;
  printParamsSource(params, to);

  if (returnAnnotation) {
    to << " : ";
    returnAnnotation->printSource(to);
  }

  /* A class method that declares only its signature ends here; there is no
   * body for an instance to override yet. */
  if (!body)
    return;

  to << " = { ";
  body->printSource(to);
  to << " }";
}

void DefinitionData::printSource(std::ostream &to) const {
  to << "type " << name;
  for (auto &var : vars)
    to << " " << var;

  to << " = {";
  for (auto it = constructors.begin(); it != constructors.end(); it++) {
    if (it != constructors.begin())
      to << ",";
    to << " " << (*it)->name;
    for (auto &type : (*it)->types) {
      to << " ";
      type->printSource(to);
    }
  }
  to << " }";
}

void DefinitionClass::printSource(std::ostream &to) const {
  to << "class ";
  ff::sem::printContextSource(supers, to);
  head->printSource(to);
  to << " = {" << std::endl;

  for (auto &method : methods) {
    method->printSource(to, 1);
    to << std::endl;
  }
  to << "}";
}

void DefinitionInstance::printSource(std::ostream &to) const {
  to << "instance ";
  ff::sem::printContextSource(context, to);
  head->printSource(to);
  to << " = {" << std::endl;

  for (auto &method : methods) {
    method->printSource(to, 1);
    to << std::endl;
  }
  to << "}";
}

void DefinitionGroup::printSource(std::ostream &to) const {
  for (auto &pair : defsData) {
    pair.second->printSource(to);
    to << std::endl << std::endl;
  }
  for (auto &pair : defsClass) {
    pair.second->printSource(to);
    to << std::endl << std::endl;
  }
  for (auto &instance : defsInstance) {
    instance->printSource(to);
    to << std::endl << std::endl;
  }
  for (auto &pair : defsDefn) {
    pair.second->printSource(to, 0);
    to << std::endl << std::endl;
  }
}

// ############ Structure dumping ############

/* The dump says what the tree is, not how it was written: two programs that
 * differ only in the parentheses or the spacing they used dump the same. It
 * is what a round trip is checked against. */

void DefinitionDefn::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "DEFN: " << name << std::endl;

  for (auto &pred : context) {
    printIndent(indent + 1, to);
    to << "CONTEXT: ";
    pred->printSource(to);
    to << std::endl;
  }

  for (auto &param : params) {
    printIndent(indent + 1, to);
    to << "PARAM: " << param->name;
    if (param->type) {
      to << " : ";
      param->type->printSource(to);
    }
    to << std::endl;
  }

  if (returnAnnotation) {
    printIndent(indent + 1, to);
    to << "RETURN: ";
    returnAnnotation->printSource(to);
    to << std::endl;
  }

  if (body)
    body->print(indent + 1, to);
}

void DefinitionData::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "DATA: " << name;
  for (auto &var : vars)
    to << " " << var;
  to << std::endl;

  for (auto &constructor : constructors) {
    printIndent(indent + 1, to);
    to << "CONSTRUCTOR: " << constructor->name;
    for (auto &type : constructor->types) {
      to << " ";
      type->printSource(to);
    }
    to << std::endl;
  }
}

void DefinitionClass::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "CLASS: ";
  head->printSource(to);
  to << std::endl;

  for (auto &super : supers) {
    printIndent(indent + 1, to);
    to << "SUPER: ";
    super->printSource(to);
    to << std::endl;
  }

  for (auto &method : methods)
    method->print(indent + 1, to);
}

void DefinitionInstance::print(int indent, std::ostream &to) const {
  printIndent(indent, to);
  to << "INSTANCE: ";
  head->printSource(to);
  to << std::endl;

  for (auto &param : dictionaryParams) {
    printIndent(indent + 1, to);
    to << "PARAM: " << param << std::endl;
  }

  for (auto &pred : context) {
    printIndent(indent + 1, to);
    to << "CONTEXT: ";
    pred->printSource(to);
    to << std::endl;
  }

  for (auto &method : methods)
    method->print(indent + 1, to);
}

void DefinitionGroup::print(int indent, std::ostream &to) const {
  for (auto &pair : defsData)
    pair.second->print(indent, to);
  for (auto &pair : defsClass)
    pair.second->print(indent, to);
  for (auto &instance : defsInstance)
    instance->print(indent, to);
  for (auto &pair : defsDefn)
    pair.second->print(indent, to);
}
