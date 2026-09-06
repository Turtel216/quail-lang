// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun toInt b = { if b { 1 } else { 0 } }

// Comparisons are signed, so a negative operand is below every positive one.
fun main = {
  toInt (0 - 5 < 0 - 1) * 1000 +
  toInt (0 - 1 > 0 - 5) * 100 +
  toInt (0 - 1 < 1) * 10 +
  toInt (0 - 3 == 0 - 3)
}

// CHECK: Result: 1111
