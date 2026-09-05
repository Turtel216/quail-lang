// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun main = { if 1 { 2 } else { 3 } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the condition of an if expression is not a Bool
// CHECK-SAME: its type is Int
