// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun toInt b = { if b { 1 } else { 0 } }

// A comparison binds looser than arithmetic, so both sides are worked out
// before they are compared.
fun main = {
  toInt (1 + 2 == 3) * 100 +
  toInt (2 * 3 > 5) * 10 +
  toInt (8 / 2 - 1 == 3)
}

// CHECK: Result: 111
