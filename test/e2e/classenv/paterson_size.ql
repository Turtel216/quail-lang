// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

// Reducing a wanted constraint against this instance would replace it with a
// larger one, and so need never finish.

type Bool = { True, False }

class Eq a = { fun eq (x: a) (y: a) : Bool }

instance Eq (List (List a)) => Eq (List a) = { fun eq x y = { True } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: is not smaller than what it is a condition of
