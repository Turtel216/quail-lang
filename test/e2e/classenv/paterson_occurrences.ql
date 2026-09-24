// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

// The context is no larger than the head, but it repeats a variable the head
// mentions once, which is the other way a reduction can fail to shrink.

type Bool = { True, False }
type Pair a b = { MkPair a b }
type Tree a = { Leaf, Node a }

class Eq a = { fun eq (x: a) (y: a) : Bool }

instance Eq (Pair a a) => Eq (Tree a) = { fun eq x y = { True } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: mentions a more often than Eq (Tree a) does
