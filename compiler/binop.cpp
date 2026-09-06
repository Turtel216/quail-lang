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
