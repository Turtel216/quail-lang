// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

type Answer = { Yes, No }

fun asks a = {
  match a with {
    Yes -> { True }
    No -> { False }
  }
}

// The condition is an expression like any other, so it may call a function
// or come out of a match.
fun main = {
  if not (asks No) {
    42
  } else {
    0
  }
}

// CHECK: Result: 42
