// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun toInt b = { if b { 1 } else { 0 } }

fun countTrue l = {
  match l with {
    Nil -> { 0 }
    Cons x xs -> { toInt x + countTrue xs }
  }
}

fun main = {
  let {
    fun bigger a b = { a > b }
  } in {
    countTrue [1 == 1, 2 < 1, 3 >= 3] * 10 + toInt (bigger 4 2)
  }
}

// CHECK: Result: 21
