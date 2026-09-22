#pragma once

#include <string>

enum binop {
  PLUS,
  MINUS,
  TIMES,
  DIVIDE,
  EQUALS,
  NOTEQUALS,
  LESS,
  LESSEQUALS,
  GREATER,
  GREATEREQUALS
};

std::string opName(binop op);
std::string opAction(binop op);

/* The method an operator stands for. An operator is surface syntax for a
 * method of one of the prelude's classes, so `x + y` means `add x y` and
 * carries whatever constraint `add` carries. */
std::string opMethod(binop op);

/* The name the compiler's own implementation is bound under. It works on
 * Int and nothing else, and is what the prelude's instances for Int call. */
std::string opPrimitive(binop op);

/* Comparisons take the same two Ints as the arithmetic operators but answer
 * with a Bool, so they are built and lowered differently. */
bool isComparison(binop op);
