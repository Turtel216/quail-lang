// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class C a = { fun f (x: Bool) : Bool }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the signature of f does not mention a
