// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun pick b = {
  if b {
    2
  } else {
    3
  }
}

fun main = { pick True * 20 + pick False - 1 }

// CHECK: Result: 42
