// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// The condition and both branches see the enclosing scope, so lifting a
// lambda or a let binding has to carry what they mention along.
fun main = {
  let {
    fun scale n = { map (\x -> { if x { n } else { 0 - n } }) }
    fun total l = {
      match l with {
        Nil -> { 0 }
        Cons x xs -> { x + total xs }
      }
    }
  } in {
    [True, False, True, True] |> scale 21 |> total
  }
}

// CHECK: Result: 42
