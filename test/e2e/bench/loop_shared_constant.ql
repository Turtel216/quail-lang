// RUN: %qc %s -o %t
// RUN: env QUAIL_STATS=1 %t 2>&1 | FileCheck %s

// The same, for a dictionary handed to an overloaded function rather than
// selected from. There is no method reference to shortcut here, so what
// keeps this out of the loop is naming the dictionary and sharing it: it
// depends on nothing, so it is the same dictionary at every iteration.
//
// The loop says what it is about. Left to inference it would be about any
// number at all, and the dictionary would then depend on which, so there
// would be nothing constant to share.

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

fun Same a => anyEq (x: a) (xs: List a) : Bool = {
    match xs with {
        Nil -> { False }
        Cons y ys -> { if same x y { True } else { anyEq x ys } }
    }
}

fun loop (n: Int) (acc: Int) : Int = { if n <= 0 { acc } else { loop (n - 1) (if anyEq [n] [[n]] { acc + 1 } else { acc }) } }

fun main = { loop 1000 0 }

// CHECK: inds 3{{$}}
// CHECK: Result: 1000
