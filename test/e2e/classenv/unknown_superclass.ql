// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class Nope a => C a = { fun f (x: a) : Bool }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: unknown class Nope in the superclasses of C
