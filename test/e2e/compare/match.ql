// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A comparison answers with an ordinary Bool, so it can be taken apart by
// naming the two constructors.
fun describe n = {
  match n > 0 with {
    True -> { 1 }
    False -> { 2 }
  }
}

fun main = { describe 5 * 10 + describe (0 - 5) }

// CHECK: Result: 12
