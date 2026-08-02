// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun list = { Cons 42 (Cons 1 (Cons 1 (Cons 1 Nil))) }

fun main = {
  match head list with {
    Nothing -> { 43 }
    Just x -> { x }
  }
}

// CHECK: Result: 42
