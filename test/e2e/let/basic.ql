// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun main = {
    let {
        fun square x = { x * x }
    } in {
        square 5 + square 4 + 1
    }
}

// CHECK: Result: 42
