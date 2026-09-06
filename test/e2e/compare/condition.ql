// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun count n = {
  if n <= 0 {
    0
  } else {
    1 + count (n - 1)
  }
}

fun main = { count 5 * 10 + count 0 }

// CHECK: Result: 50
