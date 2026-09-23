// RUN: %qc %s --dump-types | FileCheck %s

// A let binding cannot answer for a constraint about a variable the function
// around it fixed, so the constraint is handed outward and shows up on the
// function instead.

class Same a = { fun same (x: a) (y: a) : Bool }
instance Same Int = { fun same x y = { x == y } }

fun outer x y = {
    let {
        fun withY p = { same p y }
    } in {
        withY x
    }
}

// CHECK: outer : Same a => a -> a -> Bool
