// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }
type Tree a = { Leaf, Node a }

class Eq a = { fun eq (x: a) (y: a) : Bool }

instance Eq b => Eq (Tree a) = { fun eq x y = { True } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: constrains b, which the instance is not about
