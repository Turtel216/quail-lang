// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun mul x y = { x * y }

fun main = { mul 7 6 }

// CHECK: Result: 42
