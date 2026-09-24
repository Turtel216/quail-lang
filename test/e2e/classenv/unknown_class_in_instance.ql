// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

instance Nope Int = { fun eq x y = { x } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: unknown class Nope in an instance declaration
