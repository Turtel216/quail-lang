// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun main = {
    let {
        fun list = { [1, 2, 3, 4] }
        fun skipAdd x y = { y + 1 }
    } in {
        list |> foldr skipAdd 0
    }
}

// CHECK: Result: 4
