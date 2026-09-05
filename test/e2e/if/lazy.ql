// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// Only the branch that is taken is ever evaluated; the other one would
// divide by zero.
fun choose b x = {
  if b {
    x + 2
  } else {
    x / 0
  }
}

fun main = { choose True 40 }

// CHECK: Result: 42
