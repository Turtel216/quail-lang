// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// Comparison is a class method now, so it works on anything with an
// instance, and Bool has one. What it does not work on is a type nothing
// declared an instance for.

type Colour = { Red, Green }

fun main = { if Red == Green { 1 } else { 0 } }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: no instance for Eq Colour, arising from a use of ==
