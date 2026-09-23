// RUN: %qc %s --dump-core | FileCheck %s

// A constraint about a known type is answered by the instance that has it,
// built from evidence for whatever that instance needs in turn.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Same a => Same (List a) = { fun same xs ys = { True } }

fun atInt = { same 1 2 }
fun atList = { same [1] [2] }
fun atNested = { same [[1]] [[2]] }

// CHECK: DEFN: atInt
// CHECK: DICT: Same$Int$inst
// CHECK: DEFN: atList
// CHECK: DICT: (Same$List$inst Same$Int$inst)
// CHECK: DEFN: atNested
// CHECK: DICT: (Same$List$inst (Same$List$inst Same$Int$inst))
