// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun countTrue l = {
  match l with {
    Nil -> { 0 }
    Cons x xs -> { if x (1 + countTrue xs) (countTrue xs) }
  }
}

fun main = {
  countTrue [True, True, False, True]
}

// CHECK: Result: 3
