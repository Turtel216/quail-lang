// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// The return type may be written on its own, with or without parameters.
fun answer : Int = { 42 }

fun twice x : Int = { x + x }

fun main = { answer + twice 0 }

// CHECK: Result: 42
