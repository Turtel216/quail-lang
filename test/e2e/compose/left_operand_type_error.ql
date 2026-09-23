// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun inc x = { x + 1 }

fun main = { (2 . inc) 1 }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the left side of . is not a function, it is a number
