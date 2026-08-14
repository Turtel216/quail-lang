// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun list = { Cons 1 (Cons 2 (Cons 3 Nil)) }

fun sub x y = { x - y }

fun main = {
  foldr sub 0 list
}

// CHECK: Result: 2
