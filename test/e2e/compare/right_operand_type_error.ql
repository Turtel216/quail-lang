// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun main = { if 1 < True { 1 } else { 0 } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the right operand of < is not Int
// CHECK-SAME: its type is Bool
