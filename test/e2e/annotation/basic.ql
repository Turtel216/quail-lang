// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add (x: Int) (y: Int) : Int = { x + y }

fun main = { add 15 27 }

// CHECK: Result: 42
