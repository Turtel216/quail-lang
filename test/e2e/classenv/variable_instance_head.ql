// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

// An instance says what is true of every type of one shape, so its head has
// to name that shape.

type Bool = { True, False }

class Eq a = { fun eq (x: a) (y: a) : Bool }

instance Eq a = { fun eq x y = { True } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the instance head Eq a is not a type constructor applied to distinct type variables
