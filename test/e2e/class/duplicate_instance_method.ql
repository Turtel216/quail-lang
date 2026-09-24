// RUN: not %qc %s --dump-ast 2>&1 | FileCheck %s

instance Eq Int = {
    fun eq x y = { True }
    fun eq x y = { False }
}

// CHECK: an error occured while compiling the program
// CHECK-SAME: the method eq is written twice in this instance
