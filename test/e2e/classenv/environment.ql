// RUN: %qc %s --check-classes | FileCheck %s

// What a well formed set of declarations comes to: the class variable, the
// superclasses, the methods in the order the class wrote them, which of them
// carry a default, and the instances.

type Bool = { True, False }
type Pair a b = { MkPair a b }

class Eq a = {
    fun eq (x: a) (y: a) : Bool
    fun neq (x: a) (y: a) : Bool = { True }
}

class Eq a => Ord a = {
    fun le (x: a) (y: a) : Bool
}

instance Eq Int = {
    fun eq x y = { True }
}

instance Eq a => Eq (List a) = {
    fun eq xs ys = { True }
}

instance (Eq a, Eq b) => Eq (Pair a b) = {
    fun eq p q = { True }
}

instance Ord Int = {
    fun le x y = { True }
}

// CHECK: CLASS Eq a
// CHECK-NEXT: METHOD eq : a -> a -> Bool
// CHECK-NEXT: DEFAULT neq : a -> a -> Bool
// CHECK-NEXT: INSTANCE Eq Int
// CHECK-NEXT: INSTANCE (Eq a) => Eq (List a)
// CHECK-NEXT: INSTANCE (Eq a, Eq b) => Eq (Pair a b)
// CHECK-NEXT: CLASS Ord a
// CHECK-NEXT: SUPER Eq a
// CHECK-NEXT: METHOD le : a -> a -> Bool
// CHECK-NEXT: INSTANCE Ord Int
