// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun main = { if 1 < 2 < 3 { 1 } else { 0 } }

// CHECK: an error occured while compiling the program
// CHECK-SAME: comparison operators do not chain
