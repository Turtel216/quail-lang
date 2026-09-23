// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// One overloaded definition, used at two types in the same body, is one
// function applied to two different dictionaries.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Same a => Same (List a) = { fun same xs ys = { True } }

fun member x xs = {
    match xs with {
        Nil -> { False }
        Cons y ys -> { if same x y { True } else { member x ys } }
    }
}

fun main = {
    if member 2 [1, 2, 3] {
        if member [9] [[1], [9]] { 42 } else { 0 }
    } else { 0 }
}

// CHECK: Result: 42
