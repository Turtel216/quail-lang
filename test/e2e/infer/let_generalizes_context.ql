// RUN: %qc %s --dump-types | FileCheck %s

// A let binding whose constraint is about a variable the function around it
// has not fixed keeps the constraint and is generalized over it, so it may
// be used at two types in the same body.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Same a => Same (List a) = { fun same xs ys = { True } }

fun outer = {
    let {
        fun poly p q = { same p q }
    } in {
        if poly 1 2 { poly [1] [1] } else { False }
    }
}

// CHECK: outer : Bool
