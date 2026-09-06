// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun toInt b = { if b { 1 } else { 0 } }

fun main = {
  toInt (1 == 1) * 100000 +
  toInt (1 == 2) * 10000 +
  toInt (1 != 2) * 1000 +
  toInt (2 != 2) * 100 +
  toInt (3 > 2) * 10 +
  toInt (2 > 3)
}

// CHECK: Result: 101010
