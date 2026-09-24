// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class Eq a = { fun eq (x: a) (y: a) : Bool }

instance Eq Int = { fun eq x y = { True } }
instance Eq Int = { fun eq x y = { False } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the instance Eq Int overlaps the instance Eq Int
