// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }

fun isEmpty l = {
  match l with {
    Nil -> { 1 }
    Cons x xs -> { 0 }
  }
}

fun main = {
  foldr add 0 [] + isEmpty [] + isEmpty [1]
}

// CHECK: Result: 1
