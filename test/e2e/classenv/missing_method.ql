// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class Eq a = {
    fun eq (x: a) (y: a) : Bool
    fun neq (x: a) (y: a) : Bool
}

instance Eq Int = { fun eq x y = { True } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the instance Eq Int does not define neq, and Eq gives it no default implementation
