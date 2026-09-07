// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun nothing (x: Int) : Maybe = { Nothing }

fun main = { 1 }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: invalid application of type Maybe
// CHECK-SAME: 1 argument(s) expected, but 0 provided
// CHECK: : Maybe
