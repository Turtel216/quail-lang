// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun length l = { foldr (\x acc -> { acc + 1 }) 0 l }

// id is used at both List Int and Int, so the let binding must generalize.
fun main = {
    let {
        fun id x = { x }
    } in {
        id 40 + length (id (Cons 1 (Cons 2 Nil)))
    }
}

// CHECK: Result: 42
