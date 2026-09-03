// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }
fun sum l = { foldr add 0 l }

fun main = {
  let {
    fun pair a b = { [a, b, a + b] }
  } in {
    sum [1 + 1, 2 * 3, sum [4]] + sum (pair 1 2) + sum (map (\x -> { sum [x, x] }) [3, 4])
  }
}

// CHECK: Result: 32
