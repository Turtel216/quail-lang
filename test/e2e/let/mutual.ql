// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun list = { Cons 1 (Cons 2 (Cons 3 Nil)) }

fun main = {
    let {
        fun evenLen l = { match l with { Nil -> { 42 } Cons x xs -> { oddLen xs } } }
        fun oddLen l = { match l with { Nil -> { 0 } Cons x xs -> { evenLen xs } } }
    } in {
        evenLen list + oddLen list
    }
}

// CHECK: Result: 42
