#pragma once

#include <string>

enum binop { PLUS, MINUS, TIMES, DIVIDE };

std::string opName(binop op);
std::string opAction(binop op);
