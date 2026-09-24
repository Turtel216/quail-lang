// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// An instance that holds under another: the dictionary for lists of a is
// built from the dictionary for a.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }

instance Same a => Same (List a) = {
    fun same xs ys = {
        match xs with {
            Nil -> { match ys with { Nil -> { True } Cons y more -> { False } } }
            Cons x rest -> {
                match ys with {
                    Nil -> { False }
                    Cons y more -> { if same x y { same rest more } else { False } }
                }
            }
        }
    }
}

fun main = {
    if same [1, 2, 3] [1, 2, 3] {
        if same [1, 2] [1, 3] { 0 } else { 42 }
    } else { 0 }
}

// CHECK: Result: 42
