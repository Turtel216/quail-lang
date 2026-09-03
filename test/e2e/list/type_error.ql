// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun main = { [1, True] }

// CHECK: an error occured while checking the types of the program
// CHECK: the expected type was:
// CHECK: Int
// CHECK: while the actual type was:
// CHECK: Bool
