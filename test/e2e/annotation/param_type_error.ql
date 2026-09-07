// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// An annotated parameter is that type everywhere in the body, so the use
// that disagrees is the one reported.
fun bad (b: Bool) : Int = { b + 1 }

fun main = { bad True }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the left operand of + is not Int
// CHECK-SAME: its type is Bool
