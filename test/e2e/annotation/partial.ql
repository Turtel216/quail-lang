// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// Annotations are optional one at a time: what is left off is inferred.
fun scale n (factor: Int) = { n * factor }

fun offset (n: Int) m = { n + m }

fun main = { scale 4 10 + offset 1 1 }

// CHECK: Result: 42
