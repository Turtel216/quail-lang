// RUN: %qc %s --dump-core | FileCheck %s

// A let binding generalized over a constraint takes a dictionary of its own
// and is used at two of them. A lambda takes none, and reaches for the one
// the definition around it was handed.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Same a => Same (List a) = { fun same xs ys = { True } }

fun twoTypes = {
    let {
        fun poly p q = { same p q }
    } in {
        if poly 1 2 { poly [1] [1] } else { False }
    }
}

fun inLambda x xs = { foldr (\y acc -> { if same x y { True } else { acc } }) False xs }

// Definitions are dumped in name order, so inLambda comes first.
// CHECK: DEFN: inLambda
// CHECK-NEXT: PARAM: d$Same$0
// CHECK: LAMBDA: y acc
// CHECK: LID: same
// CHECK-NEXT: DICT: d$Same$0
// CHECK: DEFN: twoTypes
// CHECK: poly d$Same$0 p q:
// CHECK: LID: poly
// CHECK-NEXT: DICT: Same$Int$inst
// CHECK: LID: poly
// CHECK-NEXT: DICT: (Same$List$inst Same$Int$inst)
