// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A dictionary parameter makes a definition take one more argument than it
// was written with, so an application that looked saturated is now partial.
// Nothing special happens: an under applied global is an application spine
// like any other, and unwinding finishes it when the rest arrives.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }

fun member x xs = {
    match xs with {
        Nil -> { False }
        Cons y ys -> { if same x y { True } else { member x ys } }
    }
}

fun apply f v = { f v }

fun main = {
    let {
        fun hasTwo = { apply (member 2) [1, 2, 3] }
        fun hasNine = { apply (member 9) [1, 2, 3] }
    } in {
        if hasTwo { if hasNine { 0 } else { 42 } } else { 0 }
    }
}

// CHECK: Result: 42
