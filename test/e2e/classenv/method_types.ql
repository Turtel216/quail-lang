// RUN: %qc %s --check-classes | FileCheck %s

// A method may quantify over more than its class does, and may take and hand
// back functions and applied types, so long as it mentions the class
// variable somewhere.

type Bool = { True, False }

class Container a = {
    fun empty : a
    fun insert (x: Int) (c: a) : a
    fun fold (f: Int -> b -> b) (seed: b) (c: a) : b
    fun items (c: a) : List Int
}

instance Container (List a) = {
    fun empty = { Nil }
    fun insert x c = { Cons x c }
    fun fold f seed c = { seed }
    fun items c = { c }
}

// CHECK: CLASS Container a
// CHECK-NEXT: METHOD empty : a
// CHECK-NEXT: METHOD insert : Int -> a -> a
// CHECK-NEXT: METHOD fold : (Int -> (b -> b)) -> b -> a -> b
// CHECK-NEXT: METHOD items : a -> (List Int)
// CHECK-NEXT: INSTANCE Container (List a)
