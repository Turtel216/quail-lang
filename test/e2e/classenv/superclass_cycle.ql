// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class D a => C a = { fun f (x: a) : Bool }
class C a => D a = { fun g (x: a) : Bool }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the superclasses of C run in a circle: C -> D -> C
