// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }
fun double x = { x * 2 }

// Application binds tighter than composition, so the right side is add 1,
// and composition binds tighter than the arithmetic, so the sum is of 40
// and the composed function applied to 0.
fun main = { 40 + (double . add 1) 0 }

// CHECK: Result: 42
