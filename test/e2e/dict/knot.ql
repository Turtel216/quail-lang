// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// The dictionary an instance builds is a field of itself, because the
// default it holds has to be able to call back into it. Nothing forces it
// while it is being built, so the knot ties instead of looping. Reaching the
// default through the recursive list instance walks that knot twice over.

class Same a = {
    fun same (x: a) (y: a) : Bool
    fun differs (x: a) (y: a) : Bool = { not (same x y) }
}

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
    if differs [[1]] [[2]] {
        if differs [[3]] [[3]] { 0 } else { 42 }
    } else { 0 }
}

// CHECK: Result: 42
