// RUN: %qc %s --check-classes | FileCheck %s

// An instance that takes every default writes no methods at all, and a class
// that only gathers superclasses declares none.

type Bool = { True, False }

class Eq a = {
    fun eq (x: a) (y: a) : Bool = { True }
}

class Eq a => Sorted a = { }

instance Eq Int = { }

// CHECK: CLASS Eq a
// CHECK-NEXT: DEFAULT eq : a -> a -> Bool
// CHECK-NEXT: INSTANCE Eq Int
// CHECK-NEXT: CLASS Sorted a
// CHECK-NEXT: SUPER Eq a
