// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

// A head that pins one of its arguments down would overlap the general
// instance of the same constructor, so heads take variables only.

type Bool = { True, False }

class Eq a = { fun eq (x: a) (y: a) : Bool }

instance Eq (List Int) = { fun eq x y = { True } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the instance head Eq (List Int) applies Eq to a type where it needs a type variable
