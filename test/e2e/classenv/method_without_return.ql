// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class C a = { fun f (x: a) }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the method f does not say what it hands back
