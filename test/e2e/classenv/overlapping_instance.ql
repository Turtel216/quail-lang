// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

// Two instances of one class over the same constructor could both solve a
// constraint, whatever their contexts say.

type Bool = { True, False }

class Eq a = { fun eq (x: a) (y: a) : Bool }

instance Eq (List a) = { fun eq x y = { True } }
instance Eq a => Eq (List a) = { fun eq x y = { False } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: overlaps the instance Eq (List a)
