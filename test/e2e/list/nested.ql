// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }
fun sum l = { foldr add 0 l }

fun sumAll l = {
  match l with {
    Nil -> { 0 }
    Cons x xs -> { sum x + sumAll xs }
  }
}

fun main = {
  sumAll [[1, 2], [3, 4], []]
}

// CHECK: Result: 10
