// RUN: not %qc %s --dump-ast 2>&1 | FileCheck %s

class Eq a = {
    fun eq (x: a) (y: a) : Bool
    fun eq (x: a) (y: a) : Bool = { True }
}

// CHECK: an error occured while compiling the program
// CHECK-SAME: the method eq is written twice in this class
