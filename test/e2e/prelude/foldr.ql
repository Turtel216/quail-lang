// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun list = { [1, 2, 3] }

fun sub x y = { x - y }

fun main = {
  foldr sub 0 list
}

// CHECK: Result: 2
