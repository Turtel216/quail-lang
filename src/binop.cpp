#include "../include/binop.hpp"

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
  }
  return "??";
}
