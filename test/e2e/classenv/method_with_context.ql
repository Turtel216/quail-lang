// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class Eq a = { fun eq (x: a) (y: a) : Bool }
class C a = { fun f (x: a) : Bool = { True } }

instance Eq Int = { fun Eq b => eq x y = { True } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the method eq of instance Eq Int writes a context of its own
