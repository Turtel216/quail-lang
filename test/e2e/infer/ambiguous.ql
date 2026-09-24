// RUN: not %qc %s --dump-types 2>&1 | FileCheck %s

// Reading and then showing says nothing about what was in between, and no
// caller can say either, so the type in the middle is ambiguous. Neither
// class is numeric, so defaulting does not apply.

class Show a = { fun show (x: a) : Int }
class Read a = { fun read (n: Int) : a }

instance Show Int = { fun show x = { x } }
instance Read Int = { fun read n = { n } }

fun roundTrip n = { show (read n) }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: is ambiguous
