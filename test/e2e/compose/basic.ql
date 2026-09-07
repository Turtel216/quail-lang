// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun double x = { x * 2 }
fun inc x = { x + 1 }

// The right side runs first and hands its answer to the left side.
fun main = { (double . inc) 20 }

// CHECK: Result: 42
