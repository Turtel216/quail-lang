// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun main = {
  match [42, 1, 1] with {
    Nil -> { 0 }
    Cons x xs -> { x }
  }
}

// CHECK: Result: 42
