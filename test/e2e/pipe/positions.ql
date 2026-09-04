// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }
fun sum l = { foldr add 0 l }

// A pipe is a whole expression, so it may stand wherever one may: inside a
// list literal, as the value a match takes apart, and in a branch.
fun main = {
  match [20, 1] |> map (add 1) with {
    Nil -> { 0 }
    Cons x xs -> { [x |> add 19, xs |> sum] |> sum }
  }
}

// CHECK: Result: 42
