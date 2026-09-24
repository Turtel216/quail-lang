// RUN: %qc %s -o %t
// RUN: env QUAIL_STATS=1 %t 2>&1 | FileCheck %s

// An overloaded call in a tight recursive loop must not build a dictionary
// per iteration. Every dictionary construction allocates exactly one
// placeholder indirection, so the count of those says how many were built,
// and nothing else in this program allocates one: there are no let blocks.
//
// The loop runs a thousand times. A count that stayed proportional to that
// would be the trap this is here to catch.

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

fun atList (n: Int) (acc: Int) : Int = { if n <= 0 { acc } else { atList (n - 1) (if same [n] [n] { acc + 1 } else { acc }) } }

fun main = { atList 1000 0 }

// CHECK: inds 1{{$}}
// CHECK: Result: 1000
