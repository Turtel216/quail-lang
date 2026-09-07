// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A let binding is an ordinary definition, so it may be annotated too, and
// what it captures does not disturb the type it declared.
fun outer (n: Int) : Int = {
    let {
        fun bump (m: Int) : Int = { m + n }
    } in {
        bump 2
    }
}

fun main = { outer 40 }

// CHECK: Result: 42
