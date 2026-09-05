// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun main = { if True { 1 } }

// CHECK: an error occured while compiling the program
// CHECK-SAME: an if expression must have an else branch
