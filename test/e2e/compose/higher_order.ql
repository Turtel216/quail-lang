// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun inc x = { x + 1 }
fun id x = { x }

// Composition works on whatever two functions fit together, so a function
// that composes its own argument stays polymorphic.
fun twice f = { f . f }

fun unwrap m = {
    match m with {
        Nothing -> { 0 }
        Just x -> { x }
    }
}

fun main = {
    let {
        fun both = { id . id }
    } in {
        unwrap ((Just . inc) 37) + (twice inc) 1 + both 1 + unwrap (both Nothing)
    }
}

// CHECK: Result: 42
