// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun inc x = { x + 1 }
fun loop x = { loop x }
fun const x y = { x }

// The composed function is only a graph until something needs its answer,
// so the argument that never ends is never asked for.
fun main = { const ((inc . inc) 40) ((inc . loop) 1) }

// CHECK: Result: 42
