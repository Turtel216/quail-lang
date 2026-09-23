// RUN: not %qc %s --dump-types 2>&1 | FileCheck %s

// The list instance holds only when the element type does, so a list of
// something with no instance has none either.

class Same a = { fun same (x: a) (y: a) : Bool }
instance Same a => Same (List a) = { fun same xs ys = { True } }

fun bad = { same [True] [False] }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: no instance for Same Bool
