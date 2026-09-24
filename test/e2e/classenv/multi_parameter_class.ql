// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class Convert a b = {
    fun convert (x: a) : b
}

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the class Convert abstracts over 2 types, but a class abstracts over exactly one
