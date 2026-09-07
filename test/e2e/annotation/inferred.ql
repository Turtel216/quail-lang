// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A definition that writes no annotation is inferred exactly as before, and
// mixes freely with ones that do.
fun double x = { x * 2 }

fun add (x: Int) (y: Int) : Int = { x + y }

fun main = { add (double 20) 2 }

// CHECK: Result: 42
