// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// Parentheses around a parameter are what asks for an annotation, so there
// is nothing left for them to mean without one.
fun f (x) : Int = { 1 }

fun main = { 1 }

// CHECK: an error occured while compiling the program
// CHECK-SAME: an annotated parameter needs a type, as in (x: Int)
