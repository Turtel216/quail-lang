// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A dictionary reaches a definition as an unevaluated graph. The first
// selector to look at it forces it and leaves the result in its place, so
// the second selector reads what the first built rather than building it
// again. Verified by counting: this program builds one dictionary.

class Same a = {
    fun same (x: a) (y: a) : Bool
    fun differs (x: a) (y: a) : Bool = { not (same x y) }
}

instance Same Int = { fun same x y = { x == y } }
instance Same a => Same (List a) = { fun same xs ys = { True } }

fun Same a => useTwice (x: a) (y: a) : Bool = { if same x y { differs x y } else { False } }

fun main = { if useTwice [1] [1] { 0 } else { 42 } }

// CHECK: Result: 42
