// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun apply (f: Int -> Int) (x: Int) : Int = { f (f x) }

fun main = { apply (\n -> { n + 21 }) 0 }

// CHECK: Result: 42
