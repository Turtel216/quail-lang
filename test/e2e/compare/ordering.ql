// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun toInt b = { if b { 1 } else { 0 } }

fun main = {
  toInt (2 < 3) * 100000 +
  toInt (3 < 2) * 10000 +
  toInt (3 >= 3) * 1000 +
  toInt (2 >= 3) * 100 +
  toInt (3 <= 3) * 10 +
  toInt (4 <= 3)
}

// CHECK: Result: 101010
