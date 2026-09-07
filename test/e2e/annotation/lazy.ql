// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun loop (n: Int) : Int = { loop n }

// An annotation says what an argument is, not that it is ever evaluated.
fun pick (a: Int) (b: Int) : Int = { a }

fun main = { pick 42 (loop 0) }

// CHECK: Result: 42
