#include "classes.hpp"

#include "ast.hpp"
#include "error.hpp"
#include <cassert>
#include <set>
#include <sstream>

namespace ff {
namespace sem {

namespace {

/* How a declaration is named in a diagnostic: as the program wrote it, so
 * that what is reported is what can be found in the file. */
std::string sourceOf(const ParsedPred &pred) {
  std::ostringstream stream;
  pred.printSource(stream);
  return stream.str();
}

std::string sourceOf(const ParsedContext &context, const ParsedPred &head) {
  std::ostringstream stream;
  printContextSource(context, stream);
  head.printSource(stream);
  return stream.str();
}

/* Every type variable a declaration writes. A class or an instance quantifies
 * over the ones it mentions, so they have to be gathered before any part of
 * it is turned into a type. */
std::set<std::string> writtenVariables(const ParsedContext &context,
                                       const ParsedPred &head) {
  std::set<std::string> written;
  head.collectVariables(written);
  for (auto &pred : context)
    pred->collectVariables(written);
  return written;
}

/* The number of type constructors and type variables a type is built from,
 * counting repetitions. Paterson's condition is stated in terms of it. */
std::size_t typeSize(const std::shared_ptr<Type> &type) {
  if (auto *arr = dynamic_cast<TypeArr *>(type.get()))
    return 1 + typeSize(arr->getLeft()) + typeSize(arr->getRight());

  if (auto *app = dynamic_cast<TypeApp *>(type.get())) {
    std::size_t size = 1;
    for (auto &argument : app->arguments)
      size += typeSize(argument);
    return size;
  }

  return 1;
}

std::size_t countVariable(const std::shared_ptr<Type> &type,
                          const std::string &name) {
  if (auto *var = dynamic_cast<TypeVar *>(type.get()))
    return var->getName() == name ? 1 : 0;

  if (auto *arr = dynamic_cast<TypeArr *>(type.get()))
    return countVariable(arr->getLeft(), name) +
           countVariable(arr->getRight(), name);

  if (auto *app = dynamic_cast<TypeApp *>(type.get())) {
    std::size_t count = 0;
    for (auto &argument : app->arguments)
      count += countVariable(argument, name);
    return count;
  }

  return 0;
}

/* Whether two types have any type in common. A variable stands for anything,
 * so it meets whatever it is held against.
 *
 * This is only as exact as it is because every instance head is checked to
 * have distinct variables first: with no variable written twice on either
 * side, a variable can never be pinned down to two different types, and so
 * there is nothing to carry between the arguments as they are compared. */
bool overlaps(const std::shared_ptr<Type> &left,
              const std::shared_ptr<Type> &right) {
  if (dynamic_cast<TypeVar *>(left.get()) ||
      dynamic_cast<TypeVar *>(right.get()))
    return true;

  auto *leftArr = dynamic_cast<TypeArr *>(left.get());
  auto *rightArr = dynamic_cast<TypeArr *>(right.get());
  if (leftArr && rightArr)
    return overlaps(leftArr->getLeft(), rightArr->getLeft()) &&
           overlaps(leftArr->getRight(), rightArr->getRight());

  auto *leftApp = dynamic_cast<TypeApp *>(left.get());
  auto *rightApp = dynamic_cast<TypeApp *>(right.get());
  if (!leftApp || !rightApp)
    return false;

  auto *leftBase = dynamic_cast<TypeBase *>(leftApp->constructor.get());
  auto *rightBase = dynamic_cast<TypeBase *>(rightApp->constructor.get());
  if (!leftBase || !rightBase || leftBase->getName() != rightBase->getName() ||
      leftApp->arguments.size() != rightApp->arguments.size())
    return false;

  for (std::size_t i = 0; i < leftApp->arguments.size(); i++) {
    if (!overlaps(leftApp->arguments[i], rightApp->arguments[i]))
      return false;
  }
  return true;
}

/* The type a method signature spells out, read off the annotations the class
 * wrote. Every part of it has to be written: a class method has no body to
 * infer the rest from, and an instance is checked against this and nothing
 * else. */
std::shared_ptr<Type> methodType(const DefinitionDefn &method,
                                 const std::set<std::string> &written,
                                 const TypeContext &typeCtx) {
  if (!method.returnAnnotation)
    throw ff::TypeError("the method " + method.name +
                            " does not say what it hands back",
                        method.loc);

  auto type = method.returnAnnotation->toType(written, typeCtx,
                                              method.returnAnnotationLoc);

  for (auto it = method.params.rbegin(); it != method.params.rend(); it++) {
    auto &param = **it;
    if (!param.type)
      throw ff::TypeError("the parameter " + param.name + " of the method " +
                              method.name + " does not say what it takes",
                          param.loc);

    type = std::shared_ptr<Type>(
        new TypeArr(param.type->toType(written, typeCtx, param.loc),
                    std::move(type)));
  }

  return type;
}

/* Write a method's declared type back out as source, for the dump. */
void printMethodType(const DefinitionDefn &method, std::ostream &to) {
  for (auto &param : method.params) {
    assert(param->type != nullptr);
    param->type->printSource(to);
    to << " -> ";
  }
  assert(method.returnAnnotation != nullptr);
  method.returnAnnotation->printSource(to);
}

} // namespace

bool MethodInfo::hasDefault() const noexcept {
  return this->declaration->body != nullptr;
}

const MethodInfo *ClassInfo::lookupMethod(const std::string &methodName) const {
  for (auto &method : methods) {
    if (method.name == methodName)
      return &method;
  }
  return nullptr;
}

const ClassInfo *ClassEnv::lookup(const std::string &name) const {
  auto it = classes.find(name);
  return it == classes.end() ? nullptr : it->second.get();
}

ClassInfo *ClassEnv::findMutable(const std::string &name) {
  auto it = classes.find(name);
  return it == classes.end() ? nullptr : it->second.get();
}

const std::string *ClassEnv::methodOwner(const std::string &methodName) const {
  auto it = methodOwners.find(methodName);
  return it == methodOwners.end() ? nullptr : &it->second;
}

/* A class head is written the way a constraint is, so that a head over the
 * wrong number of arguments parses. This is where that is answered. */
void ClassEnv::declareClasses(const DefinitionGroup &group) {
  for (auto &pair : group.defsClass) {
    auto &declaration = *pair.second;
    auto &head = *declaration.head;

    if (head.arguments.size() != 1)
      throw ff::TypeError(
          "the class " + head.className +
              " abstracts over " + std::to_string(head.arguments.size()) +
              " types, but a class abstracts over exactly one",
          head.loc);

    auto *var = dynamic_cast<ParsedTypeVar *>(head.arguments.front().get());
    if (!var)
      throw ff::TypeError("the class " + head.className +
                              " must abstract over a type variable, as in " +
                              head.className + " a",
                          head.loc);

    classes[head.className] = std::unique_ptr<ClassInfo>(
        new ClassInfo(head.className, var->var, &declaration));
  }
}

/* A superclass says what an instance of this class must already be an
 * instance of, so it can only constrain the variable this class is about. */
void ClassEnv::resolveSupers(const DefinitionGroup &group) {
  for (auto &pair : group.defsClass) {
    auto &declaration = *pair.second;
    auto &info = *classes.at(declaration.getName());

    for (auto &super : declaration.supers) {
      if (!lookup(super->className))
        throw ff::TypeError("unknown class " + super->className +
                                " in the superclasses of " + info.name,
                            super->loc);

      if (super->arguments.size() != 1)
        throw ff::TypeError("the superclass " + sourceOf(*super) + " of " +
                                info.name + " applies " + super->className +
                                " to more than one type",
                            super->loc);

      auto *var = dynamic_cast<ParsedTypeVar *>(super->arguments.front().get());
      if (!var || var->var != info.var)
        throw ff::TypeError("the superclass " + sourceOf(*super) + " of " +
                                info.name + " must constrain " + info.var +
                                ", the type " + info.name + " abstracts over",
                            super->loc);

      info.supers.push_back(
          Pred(super->className, std::shared_ptr<Type>(new TypeVar(info.var))));
    }
  }
}

/* A cycle among the superclasses would make every class in it require the
 * others before any of them could be built, so it is reported as the cycle
 * it is rather than found later as a dictionary that cannot be constructed. */
void ClassEnv::checkSuperCycles() const {
  enum class Mark { Unvisited, InProgress, Done };
  std::map<std::string, Mark> marks;
  std::vector<std::string> path;

  auto visit = [&](auto &&self, const ClassInfo &info) -> void {
    auto &mark = marks[info.name];
    if (mark == Mark::Done)
      return;

    path.push_back(info.name);
    if (mark == Mark::InProgress) {
      std::string cycle;
      /* The path from where the class was first entered, so what is reported
       * is the cycle itself and not the way in to it. */
      bool inCycle = false;
      for (auto &step : path) {
        if (step == info.name)
          inCycle = true;
        if (!inCycle)
          continue;
        if (!cycle.empty())
          cycle += " -> ";
        cycle += step;
      }
      throw ff::TypeError("the superclasses of " + info.name +
                              " run in a circle: " + cycle,
                          info.declaration->loc);
    }

    mark = Mark::InProgress;
    for (auto &super : info.supers)
      self(self, *classes.at(super.className));
    mark = Mark::Done;
    path.pop_back();
  };

  for (auto &pair : classes)
    visit(visit, *pair.second);
}

void ClassEnv::resolveMethods(const DefinitionGroup &group,
                              const TypeContext &typeCtx) {
  for (auto &pair : group.defsClass) {
    auto &declaration = *pair.second;
    auto &info = *classes.at(declaration.getName());

    for (auto &method : declaration.methods) {
      if (!method->context.empty())
        throw ff::TypeError(
            "the method " + method->name + " of " + info.name +
                " writes a context of its own; a method is already constrained "
                "by the class it belongs to",
            method->loc);

      auto owner = methodOwners.find(method->name);
      if (owner != methodOwners.end())
        throw ff::TypeError("the method " + method->name +
                                " is already a method of " + owner->second +
                                "; a use of it would not say which was meant",
                            method->loc);

      std::set<std::string> written;
      for (auto &param : method->params) {
        if (param->type)
          param->type->collectVariables(written);
      }
      if (method->returnAnnotation)
        method->returnAnnotation->collectVariables(written);

      if (written.find(info.var) == written.end())
        throw ff::TypeError("the signature of " + method->name +
                                " does not mention " + info.var + ", the type " +
                                info.name + " abstracts over",
                            method->loc);

      auto type = methodType(*method, written, typeCtx);

      methodOwners[method->name] = info.name;
      info.methods.push_back(
          MethodInfo(method->name, std::move(type),
                     std::vector<std::string>(written.begin(), written.end()),
                     method.get()));
    }
  }
}

void ClassEnv::resolveInstances(const DefinitionGroup &group,
                                const TypeContext &typeCtx) {
  for (auto &declaration : group.defsInstance) {
    auto &head = *declaration->head;

    auto *info = findMutable(head.className);
    if (!info)
      throw ff::TypeError("unknown class " + head.className +
                              " in an instance declaration",
                          head.loc);

    if (head.arguments.size() != 1)
      throw ff::TypeError("the instance head " + sourceOf(head) + " applies " +
                              head.className + " to " +
                              std::to_string(head.arguments.size()) +
                              " types, but " + head.className +
                              " abstracts over exactly one",
                          head.loc);

    auto written = writtenVariables(declaration->context, head);
    auto headType = head.arguments.front()->toType(written, typeCtx, head.loc);

    /* An instance says what is true of every type of one shape, so its head
     * has to name that shape: a constructor, and nothing standing in for one
     * argument twice. */
    auto *headApp = dynamic_cast<TypeApp *>(headType.get());
    if (!headApp)
      throw ff::TypeError(
          "the instance head " + sourceOf(head) +
              " is not a type constructor applied to distinct type variables",
          head.loc);

    std::set<std::string> headVars;
    for (auto &argument : headApp->arguments) {
      auto *var = dynamic_cast<TypeVar *>(argument.get());
      if (!var)
        throw ff::TypeError("the instance head " + sourceOf(head) +
                                " applies " + head.className +
                                " to a type where it needs a type variable",
                            head.loc);

      if (!headVars.insert(var->getName()).second)
        throw ff::TypeError("the instance head " + sourceOf(head) +
                                " writes the type variable " + var->getName() +
                                " twice; its arguments must be distinct",
                            head.loc);
    }

    std::vector<Pred> context;
    for (auto &pred : declaration->context) {
      if (!lookup(pred->className))
        throw ff::TypeError("unknown class " + pred->className +
                                " in the context of instance " +
                                sourceOf(head),
                            pred->loc);

      if (pred->arguments.size() != 1)
        throw ff::TypeError("the constraint " + sourceOf(*pred) +
                                " applies " + pred->className +
                                " to more than one type",
                            pred->loc);

      auto predType =
          pred->arguments.front()->toType(written, typeCtx, pred->loc);

      /* Paterson's conditions. Reducing a wanted constraint against this
       * instance replaces it with its context, so the context has to be
       * smaller than the head in both of the ways a type can grow, or the
       * reduction could go on forever. */
      for (auto &name : written) {
        std::size_t inPred = countVariable(predType, name);
        std::size_t inHead = countVariable(headType, name);
        if (inPred <= inHead)
          continue;

        if (inHead == 0)
          throw ff::TypeError("the constraint " + sourceOf(*pred) +
                                  " of instance " + sourceOf(head) +
                                  " constrains " + name +
                                  ", which the instance is not about",
                              pred->loc);

        throw ff::TypeError(
            "the constraint " + sourceOf(*pred) + " of instance " +
                sourceOf(head) + " mentions " + name + " more often than " +
                sourceOf(head) + " does, so reducing it need never finish",
            pred->loc);
      }

      if (typeSize(predType) >= typeSize(headType))
        throw ff::TypeError("the constraint " + sourceOf(*pred) +
                                " of instance " + sourceOf(head) +
                                " is not smaller than what it is a condition "
                                "of, so reducing it need never finish",
                            pred->loc);

      context.push_back(Pred(pred->className, std::move(predType)));
    }

    info->instances.push_back(InstanceInfo(
        std::vector<std::string>(written.begin(), written.end()),
        Qual<Pred>(std::move(context), Pred(head.className, headType)),
        declaration.get()));
  }
}

/* Two instances that a constraint could be solved by either of would leave
 * the choice of dictionary to whichever was looked at first. */
void ClassEnv::checkOverlap() const {
  for (auto &pair : classes) {
    auto &instances = pair.second->instances;
    for (std::size_t i = 0; i < instances.size(); i++) {
      for (std::size_t j = i + 1; j < instances.size(); j++) {
        if (!overlaps(instances[i].qual.head.type, instances[j].qual.head.type))
          continue;

        auto &first = *instances[i].declaration;
        auto &second = *instances[j].declaration;
        throw ff::TypeError(
            "the instance " + sourceOf(second.context, *second.head) +
                " overlaps the instance " +
                sourceOf(first.context, *first.head) + " declared on line " +
                std::to_string(first.loc.begin.line) +
                ", so a constraint could be solved by either",
            second.loc);
      }
    }
  }
}

void ClassEnv::checkInstanceMethods() const {
  for (auto &pair : classes) {
    auto &info = *pair.second;

    for (auto &instance : info.instances) {
      auto &declaration = *instance.declaration;
      std::set<std::string> provided;

      for (auto &method : declaration.methods) {
        if (!method->context.empty())
          throw ff::TypeError(
              "the method " + method->name + " of instance " +
                  sourceOf(*declaration.head) +
                  " writes a context of its own; an instance method is "
                  "constrained by the instance it belongs to",
              method->loc);

        if (!info.lookupMethod(method->name))
          throw ff::TypeError("the method " + method->name +
                                  " is not a method of " + info.name,
                              method->loc);

        provided.insert(method->name);
      }

      for (auto &method : info.methods) {
        if (provided.find(method.name) != provided.end() || method.hasDefault())
          continue;

        throw ff::TypeError("the instance " + sourceOf(*declaration.head) +
                                " does not define " + method.name +
                                ", and " + info.name +
                                " gives it no default implementation",
                            declaration.loc);
      }
    }
  }
}

void ClassEnv::build(const DefinitionGroup &group, const TypeContext &typeCtx) {
  /* Order matters: a superclass names a class, an instance names a class and
   * its methods, and an overlap is between two instances that are already
   * known to be well formed. */
  declareClasses(group);
  resolveSupers(group);
  checkSuperCycles();
  resolveMethods(group, typeCtx);
  resolveInstances(group, typeCtx);
  checkOverlap();
  checkInstanceMethods();
}

void ClassEnv::print(std::ostream &to) const {
  for (auto &pair : classes) {
    auto &info = *pair.second;
    to << "CLASS " << info.name << " " << info.var << std::endl;

    for (auto &super : info.supers)
      to << "  SUPER " << super.className << " " << info.var << std::endl;

    for (auto &method : info.methods) {
      to << (method.hasDefault() ? "  DEFAULT " : "  METHOD ") << method.name
         << " : ";
      printMethodType(*method.declaration, to);
      to << std::endl;
    }

    for (auto &instance : info.instances) {
      to << "  INSTANCE ";
      printContextSource(instance.declaration->context, to);
      instance.declaration->head->printSource(to);
      to << std::endl;
    }
  }
}

} // namespace sem
} // namespace ff
