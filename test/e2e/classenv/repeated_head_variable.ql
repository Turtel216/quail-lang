// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }
type Pair a b = { MkPair a b }

class Eq a = { fun eq (x: a) (y: a) : Bool }

instance Eq (Pair a a) = { fun eq x y = { True } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the instance head Eq (Pair a a) writes the type variable a twice
