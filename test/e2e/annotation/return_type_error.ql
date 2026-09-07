// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// A declared return type is what the definition committed to, so the body
// is held against it rather than the other way round.
fun add (x: Int) (y: Int) : Int = { True }

fun main = { add 1 2 }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the body of add does not have its declared return type
// CHECK: the expected type was:
// CHECK: Int
// CHECK: while the actual type was:
// CHECK: Bool
