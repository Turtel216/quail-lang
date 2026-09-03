// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun list = { [42, 1, 1, 1] }

fun main = {
  match head list with {
    Nothing -> { 43 }
    Just x -> { x }
  }
}

// CHECK: Result: 42
