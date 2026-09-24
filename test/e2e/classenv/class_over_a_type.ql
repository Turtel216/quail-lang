// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class Eq Int = {
    fun eq (x: Int) : Bool
}

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the class Eq must abstract over a type variable
