// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun inc x = { x + 1 }
fun isBig n = { n > 10 }

// The left side cannot take the Bool the right side hands it.
fun main = { (inc . isBig) 1 }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the left side of . takes a number, but the right side of . hands back Bool
