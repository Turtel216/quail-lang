// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A method of Ranked reaching a method of Same. The Same dictionary is taken out of
// the Ranked one that is already in hand, not solved again.

class Same a = { fun same (x: a) (y: a) : Bool }

class Same a => Ranked a = {
    fun below (x: a) (y: a) : Bool
    fun under (x: a) (y: a) : Bool = { if below x y { not (same x y) } else { False } }
}

instance Same Int = { fun same x y = { x == y } }
instance Ranked Int = { fun below x y = { x <= y } }

fun Ranked a => biggest (x: a) (y: a) : a = { if below x y { y } else { x } }

fun main = {
    if under 3 7 {
        if under 7 7 { 0 } else { biggest 40 42 }
    } else { 0 }
}

// CHECK: Result: 42
