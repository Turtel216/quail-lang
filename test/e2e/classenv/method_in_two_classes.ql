// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

// A use of a method names the method, not the class it came from, so two
// classes cannot both call one f.

type Bool = { True, False }

class C a = { fun f (x: a) : Bool }
class D a = { fun f (x: a) : Bool }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the method f is already a method of C
