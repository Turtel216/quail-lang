// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

// An instance is checked against the class signature and nothing else, so
// every part of that signature has to be written.

type Bool = { True, False }

class C a = { fun f (x: a) y : Bool }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the parameter y of the method f does not say what it takes
