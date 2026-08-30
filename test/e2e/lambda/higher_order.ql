// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun apply f v = { f v }

// A multi-parameter lambda, applied one argument at a time.
fun main = { apply (\x y -> { x * y }) 6 7 }

// CHECK: Result: 42
