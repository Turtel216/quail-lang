// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun f (x: ) : Int = { 1 }

fun main = { 1 }

// CHECK: an error occured while compiling the program
// CHECK-SAME: the parameter x is missing its type after the :
