// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

// A superclass says what an instance of this class must already be, so it
// can only constrain what this class is about.

type Bool = { True, False }

class C a = { fun f (x: a) : Bool }
class C b => D a = { fun g (x: a) : Bool }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the superclass C b of D must constrain a
