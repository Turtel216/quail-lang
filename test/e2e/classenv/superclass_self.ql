// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class C a => C a = { fun f (x: a) : Bool }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the superclasses of C run in a circle: C -> C
