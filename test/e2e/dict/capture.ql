// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A lambda takes no dictionary of its own, so it captures the one the
// definition around it was handed. A method applied to fewer arguments than
// it takes is an ordinary partial application.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }

fun countTrue l = { foldr (\b acc -> { if b { 1 + acc } else { acc } }) 0 l }

fun anyEq x xs = { foldr (\y acc -> { if same x y { True } else { acc } }) False xs }

fun main = {
    let {
        fun partial = { countTrue (map (same 2) [1, 2, 2, 3]) }
        fun captured = { if anyEq 7 [5, 6, 7] { 40 } else { 0 } }
    } in {
        partial + captured
    }
}

// CHECK: Result: 42
