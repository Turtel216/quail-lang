// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// Enough allocation to collect many times over while dictionaries are live.
// A dictionary is an ordinary node the collector traces like any other, and
// the one shared dictionary is reachable only from the slot holding it, so
// this is what says that slot is a root.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }

instance Same a => Same (List a) = {
    fun same xs ys = {
        match xs with {
            Nil -> { match ys with { Nil -> { True } Cons y m -> { False } } }
            Cons x r -> {
                match ys with {
                    Nil -> { False }
                    Cons y m -> { if same x y { same r m } else { False } }
                }
            }
        }
    }
}

fun range n = { if n <= 0 { Nil } else { Cons n (range (n - 1)) } }

fun countEq x xs = {
    match xs with {
        Nil -> { 0 }
        Cons y ys -> { if same x y { 1 + countEq x ys } else { countEq x ys } }
    }
}

fun listsEqual n = { if same (range n) (range n) { 1 } else { 0 } }

fun main = { countEq 7 (range 60000) + listsEqual 40000 * 41 }

// CHECK: Result: 42
