// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun inc x = { x + 1 }

fun main = { inc . }

// CHECK: an error occured while compiling the program
// CHECK-SAME: the . operator needs a function on its right side
