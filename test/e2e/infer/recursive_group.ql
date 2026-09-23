// RUN: %qc %s --dump-types | FileCheck %s

// Mutually recursive definitions are inferred together and come out holding
// under the same context.

class Same a = { fun same (x: a) (y: a) : Bool }
instance Same Int = { fun same x y = { x == y } }

fun evens x xs = {
    match xs with {
        Nil -> { True }
        Cons y ys -> { if same x y { odds x ys } else { False } }
    }
}

fun odds x xs = {
    match xs with {
        Nil -> { False }
        Cons y ys -> { evens x ys }
    }
}

// CHECK-DAG: evens : Same a => a -> List a -> Bool
// CHECK-DAG: odds : Same a => a -> List a -> Bool
