// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun addOne x = { x + 1 }

// The piped value does not fit the parameter the function expects.
fun main = { [1, 2] |> addOne }

// CHECK: an error occured while checking the types of the program
// CHECK: the expected type was:
// CHECK: Int
// CHECK: while the actual type was:
// CHECK: List
