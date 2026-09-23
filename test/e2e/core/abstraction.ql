// RUN: %qc %s --dump-core | FileCheck %s

// A definition that holds under a constraint takes a dictionary for it,
// ahead of the parameters it wrote, and every use of an overloaded name in
// its body is applied to that dictionary.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }

fun member x xs = {
    match xs with {
        Nil -> { False }
        Cons y ys -> { if same x y { True } else { member x ys } }
    }
}

// CHECK: DEFN: member
// CHECK-NEXT: PARAM: d$Same$0
// CHECK-NEXT: PARAM: x
// CHECK-NEXT: PARAM: xs
// CHECK: LID: same
// CHECK-NEXT: DICT: d$Same$0
// A call from inside the group passes on the dictionary the group was handed.
// CHECK: LID: member
// CHECK-NEXT: DICT: d$Same$0
