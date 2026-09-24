// RUN: not %qc %s --dump-types 2>&1 | FileCheck %s

class Same a = { fun same (x: a) (y: a) : Bool }
instance Same Int = { fun same x y = { x == y } }

fun bad = { same True True }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: no instance for Same Bool
