// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun frobnicate (x: Frob) : Int = { 1 }

fun main = { 1 }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: unknown type Frob
// CHECK: fun frobnicate
