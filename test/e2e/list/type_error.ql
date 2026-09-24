// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// A written number stands for whatever type it is used at, so a number
// where something else is wanted is reported as there being no instance of
// Num for that something else.

fun main = { [1, True] }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: no instance for Num Bool, arising from a written number
