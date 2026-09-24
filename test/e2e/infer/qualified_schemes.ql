// RUN: %qc %s --dump-types | FileCheck %s

// What inference settles on for a definition that uses an overloaded name:
// the constraint is inferred, not written, and is quantified over with the
// type it constrains.

class Same a = {
    fun same (x: a) (y: a) : Bool
    fun differs (x: a) (y: a) : Bool = { not (same x y) }
}

instance Same Int = { fun same x y = { x == y } }

fun member x xs = {
    match xs with {
        Nil -> { False }
        Cons y ys -> { if same x y { True } else { member x ys } }
    }
}

fun pairwise x y z = { if same x y { differs y z } else { False } }

// CHECK-DAG: same : Same a => a -> a -> Bool
// CHECK-DAG: differs : Same a => a -> a -> Bool
// CHECK-DAG: member : Same a => a -> List a -> Bool
// CHECK-DAG: pairwise : Same a => a -> a -> a -> Bool
