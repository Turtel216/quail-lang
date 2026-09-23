// RUN: not %qc %s --dump-types 2>&1 | FileCheck %s

// An instance method is checked against the class signature with the
// instance head standing where the class variable stood.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Same Bool = { fun same x y = { Nil } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the method same does not have the type Same declares for it
