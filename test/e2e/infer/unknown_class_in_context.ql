// RUN: not %qc %s --dump-types 2>&1 | FileCheck %s

fun Nope a => bad (x: a) : Bool = { True }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: unknown class Nope in the context of bad
