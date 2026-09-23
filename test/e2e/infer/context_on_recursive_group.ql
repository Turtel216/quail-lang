// RUN: not %qc %s --dump-types 2>&1 | FileCheck %s

// A context on one of two mutually recursive definitions would be a claim
// about both, since they are inferred together.

class Same a = { fun same (x: a) (y: a) : Bool }
instance Same Int = { fun same x y = { x == y } }

fun Same a => evens (x: a) (xs: List a) : Bool = { odds x xs }
fun odds x xs = { evens x xs }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: writes a context, but is mutually recursive with another definition
