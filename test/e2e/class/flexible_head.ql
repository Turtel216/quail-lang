// RUN: not %qc %s --dump-ast 2>&1 | FileCheck %s

// An instance head has to name a type, not stand for one.

instance Eq (f a) = {
    fun eq x y = { True }
}

// CHECK: an error occured while compiling the program
// CHECK-SAME: the type variable f cannot be applied to arguments
