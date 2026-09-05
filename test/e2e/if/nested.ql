// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// An if may stand in its own condition and in either of its branches; the
// else always belongs to the nearest if.
fun main = {
  if if True { False } else { True } {
    0
  } else {
    if False {
      1
    } else {
      if True { 42 } else { 2 }
    }
  }
}

// CHECK: Result: 42
