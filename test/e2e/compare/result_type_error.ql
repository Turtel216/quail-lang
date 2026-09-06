// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// A comparison hands back a Bool, which has no place in a sum.
fun main = { 1 + (2 == 2) }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the right operand of + is not Int
// CHECK-SAME: its type is Bool
