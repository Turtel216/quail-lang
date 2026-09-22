#include "binop.hpp"

std::string opName(binop op) {
  switch (op) {
  case PLUS:
    return "+";
  case MINUS:
    return "-";
  case TIMES:
    return "*";
  case DIVIDE:
    return "/";
  case EQUALS:
    return "==";
  case NOTEQUALS:
    return "!=";
  case LESS:
    return "<";
  case LESSEQUALS:
    return "<=";
  case GREATER:
    return ">";
  case GREATEREQUALS:
    return ">=";
  }
  return "??";
}

std::string opAction(binop op) {
  switch (op) {
  case PLUS:
    return "plus";
  case MINUS:
    return "minus";
  case TIMES:
    return "times";
  case DIVIDE:
    return "divide";
  case EQUALS:
    return "equals";
  case NOTEQUALS:
    return "notEquals";
  case LESS:
    return "less";
  case LESSEQUALS:
    return "lessEquals";
  case GREATER:
    return "greater";
  case GREATEREQUALS:
    return "greaterEquals";
  }
  return "??";
}

std::string opMethod(binop op) {
  switch (op) {
  case PLUS:
    return "add";
  case MINUS:
    return "sub";
  case TIMES:
    return "mul";
  case DIVIDE:
    return "div";
  case EQUALS:
    return "eq";
  case NOTEQUALS:
    return "neq";
  case LESS:
    return "lt";
  case LESSEQUALS:
    return "le";
  case GREATER:
    return "gt";
  case GREATEREQUALS:
    return "ge";
  }
  return "??";
}

std::string opPrimitive(binop op) {
  std::string name = opMethod(op);
  name[0] = (char)('A' + (name[0] - 'a'));
  return "prim" + name;
}

bool isComparison(binop op) {
  switch (op) {
  case EQUALS:
  case NOTEQUALS:
  case LESS:
  case LESSEQUALS:
  case GREATER:
  case GREATEREQUALS:
    return true;
  case PLUS:
  case MINUS:
  case TIMES:
  case DIVIDE:
    return false;
  }
  return false;
}
