// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }

fun main = {
  foldl add 0 [1, 2, 3, 4]
}

// CHECK: Result: 10
