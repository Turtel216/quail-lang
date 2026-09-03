// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }
fun sum l = { foldr add 0 l }

// The built-in constructors still name the same type a literal builds.
fun main = {
  sum (Cons 1 [2, 3]) + sum (Cons 4 Nil)
}

// CHECK: Result: 10
