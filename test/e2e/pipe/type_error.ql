// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// A written number stands for whatever type it is used at, so a number
// where something else is wanted is reported as there being no instance of
// Num for that something else.

fun addOne x = { x + 1 }

// The piped value does not fit the parameter the function expects.
fun main = { [1, 2] |> addOne }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: no instance for Num (List a), arising from a use of addOne
