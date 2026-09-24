// RUN: %qc %s --dump-source | FileCheck %s

// The printed form of a class and an instance, so that what a round trip is
// round tripping through is itself pinned down.

class Eq a => Ord a = {
    fun le (x: a) (y: a) : Bool
    fun lt (x: a) (y: a) : Bool = { le x y }
}

instance (Eq a, Ord a) => Ord (List a) = {
    fun le xs ys = { True }
}

// CHECK: class (Eq a) => Ord a = {
// CHECK-NEXT: fun le (x: a) (y: a) : Bool
// CHECK-NEXT: fun lt (x: a) (y: a) : Bool = { ((le x) y) }
// CHECK-NEXT: }
// CHECK: instance (Eq a, Ord a) => Ord (List a) = {
// CHECK-NEXT: fun le xs ys = { True }
// CHECK-NEXT: }
