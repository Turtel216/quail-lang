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

/* Comparisons take the same two Ints as the arithmetic operators but answer
 * with a Bool, so they are built and lowered differently. */
bool isComparison(binop op);
