// RUN: not %qc %s --dump-ast 2>&1 | FileCheck %s

fun () => f x = { x }

// CHECK: an error occured while compiling the program
// CHECK-SAME: an empty context has nothing to say
