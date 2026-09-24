// RUN: %qc %s --dump-core | FileCheck %s

// An instance that holds under a constraint takes a dictionary for it, and
// its methods reach for that. The recursive call is the class method again,
// at the element type, so it goes through this very instance.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }

instance Same a => Same (List a) = {
    fun same xs ys = {
        match xs with {
            Nil -> { True }
            Cons x rest -> {
                match ys with {
                    Nil -> { False }
                    Cons y more -> { if same x y { same rest more } else { False } }
                }
            }
        }
    }
}

// CHECK: INSTANCE: Same (List a)
// CHECK-NEXT: PARAM: d$Same$0
// CHECK: LID: same
// CHECK-NEXT: DICT: d$Same$0
// CHECK: LID: same
// CHECK-NEXT: DICT: (Same$List$inst d$Same$0)
