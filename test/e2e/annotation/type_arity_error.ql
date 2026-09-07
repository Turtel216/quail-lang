// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// List takes the type of its items, and a signature has to say which.
fun sum (xs: List) : Int = { 0 }

fun main = { 1 }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: invalid application of type List
// CHECK-SAME: 1 argument(s) expected, but 0 provided
