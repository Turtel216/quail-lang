// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun main = { 1.5 + 2 }

// CHECK: an error occured while compiling the program
// CHECK-SAME: there are no fractional numbers
