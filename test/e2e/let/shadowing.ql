// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun scale x = { x + 1 }

// Two nested bindings named scale, both shadowing the global one. Lifting
// gives each a distinct global name so the references stay separate.
fun main = {
    let { fun scale x = { x * 2 } } in {
        let { fun scale x = { x * 10 } } in { scale 2 } + scale 11
    }
}

// CHECK: Result: 42
