// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun apply f v = { f v }

// The let bindings capture the enclosing lambda's parameter, and each other.
fun main = {
    apply (\a -> {
        let { fun doubled = { a * 2 } } in {
            let { fun shifted = { doubled + a } } in { shifted + 3 }
        }
    }) 13
}

// CHECK: Result: 42
