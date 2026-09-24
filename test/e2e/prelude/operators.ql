// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// An operator means its method, and goes on meaning it where something else
// has taken the method's name. A written number is whatever type it is used
// at, so a definition without a signature is overloaded and one that takes
// no arguments is settled by the defaulting rules.

fun add x y = { x * y }

fun double x = { x + x }

fun two = { 1 + 1 }

fun main = { add 3 4 + double 11 + two * 4 }

// CHECK: Result: 42
