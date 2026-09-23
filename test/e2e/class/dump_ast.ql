// RUN: %qc %s --dump-ast | FileCheck %s

// What the dump says about a class, an instance and a constrained
// definition, spelled out so that a change to the shape of the tree is
// visible rather than only self-consistent.

class Eq a => Ord a = {
    fun le (x: a) (y: a) : Bool
}

instance Eq a => Ord (List a) = {
    fun le xs ys = { True }
}

fun Ord a => biggest (x: a) : a = { x }

// CHECK: CLASS: Ord a
// CHECK-NEXT: SUPER: Eq a
// CHECK-NEXT: DEFN: le
// CHECK-NEXT: PARAM: x : a
// CHECK-NEXT: PARAM: y : a
// CHECK-NEXT: RETURN: Bool
// CHECK: INSTANCE: Ord (List a)
// CHECK-NEXT: CONTEXT: Eq a
// CHECK-NEXT: DEFN: le
// CHECK-NEXT: PARAM: xs
// CHECK-NEXT: PARAM: ys
// CHECK-NEXT: UID: True
// CHECK: DEFN: biggest
// CHECK-NEXT: CONTEXT: Ord a
// CHECK-NEXT: PARAM: x : a
// CHECK-NEXT: RETURN: a
// CHECK-NEXT: LID: x
