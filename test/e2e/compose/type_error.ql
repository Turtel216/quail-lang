// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

fun sum l = { foldr (\x acc -> { x + acc }) 0 l }
fun inc x = { x + 1 }

// A composed function is applied like any other, and takes what its right
// side takes.
fun main = { (inc . sum) 1 }

// CHECK: an error occured while checking the types of the program
// CHECK: the expected type was:
// CHECK: List
// CHECK: while the actual type was:
// CHECK: Int
