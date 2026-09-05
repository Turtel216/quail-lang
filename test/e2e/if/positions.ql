// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }
fun sum l = { foldr add 0 l }

// An if is a whole expression, so it may stand wherever one may: inside a
// list literal, as an argument, in a pipeline, and as the value a match
// takes apart.
fun main = {
  match [if True { 20 } else { 0 }, add 1 (if False { 0 } else { 20 })] with {
    Nil -> { 0 }
    Cons x xs -> { x + (xs |> sum) + if True { 1 } else { 0 } }
  }
}

// CHECK: Result: 42
