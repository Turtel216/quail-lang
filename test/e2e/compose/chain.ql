// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun double x = { x * 2 }
fun inc x = { x + 1 }
fun half x = { x / 2 }

// Composition associates to the right, so this is double . (inc . half).
fun main = { (double . inc . half) 40 }

// CHECK: Result: 42
