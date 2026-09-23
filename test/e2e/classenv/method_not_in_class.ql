// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class Eq a = { fun eq (x: a) (y: a) : Bool }

instance Eq Int = {
    fun eq x y = { True }
    fun other x = { x }
}

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the method other is not a method of Eq
