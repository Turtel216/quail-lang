// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun sub x y = { x - y }

fun main = { sub 55 13 }

// CHECK: Result: 42
