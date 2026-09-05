// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// The two branches must agree; they are the one value the if stands for.
fun main = { if True { 1 } else { False } }

// CHECK: an error occured while checking the types of the program
// CHECK: the expected type was:
// CHECK: Int
// CHECK: while the actual type was:
// CHECK: Bool
