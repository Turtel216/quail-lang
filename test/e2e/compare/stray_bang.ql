// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun main = { 1 ! 2 }

// CHECK: an error occured while compiling the program
// CHECK-SAME: a lone ! is not an operator
