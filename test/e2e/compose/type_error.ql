// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// A written number stands for whatever type it is used at, so a number
// where something else is wanted is reported as there being no instance of
// Num for that something else.

fun sum l = { foldr (\x acc -> { x + acc }) 0 l }
fun inc x = { x + 1 }

// A composed function is applied like any other, and takes what its right
// side takes.
fun main = { (inc . sum) 1 }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: no instance for Num (List a), arising from a written number
