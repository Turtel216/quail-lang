// RUN: %qc %s -o %t
// RUN: env QUAIL_STATS=1 %t 2>&1 | FileCheck %s

// Walking a list of a thousand elements through the recursive list instance
// used to rebuild the list dictionary at every level, because the recursive
// call goes through the class rather than straight to the method. Selecting
// from a construction that is written right there is the field itself, so
// the call is direct and no dictionary is built at all.

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

fun range (n: Int) : List Int = { if n <= 0 { Nil } else { Cons n (range (n - 1)) } }

fun main = { if same (range 1000) (range 1000) { 42 } else { 0 } }

// CHECK: inds 1{{$}}
// CHECK: Result: 42
