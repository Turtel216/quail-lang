// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun main = { 1 |> 2 }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the right side of |> is not a function
// CHECK-SAME: its type is Int
