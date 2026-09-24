// RUN: %qc %s --dump-types | FileCheck %s

// A constraint about a known type is solved by the instance that answers it,
// and disappears. A nested one is solved by walking the instances down.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Same a => Same (List a) = { fun same xs ys = { True } }

fun flat = { same 1 2 }
fun nested = { same [[1]] [[2]] }
fun stillOpen xs = { same xs xs }

// CHECK-DAG: flat : Bool
// CHECK-DAG: nested : Bool
// CHECK-DAG: stillOpen : Same a => a -> Bool
