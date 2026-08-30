// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A let-bound function that calls itself: it captures its own stack slot,
// which Alloc creates before any of the bindings are filled in.
fun main = {
    let {
        fun total l = {
            match l with {
                Nil -> { 0 }
                Cons x xs -> { x + total xs }
            }
        }
    } in {
        total (Cons 20 (Cons 15 (Cons 7 Nil)))
    }
}

// CHECK: Result: 42
