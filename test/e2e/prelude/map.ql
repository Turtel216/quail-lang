// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun list = { [0, 0, 0] }

fun addOne x = { x + 1 }
fun add x y = { x + y }

fun sum l = { foldr add 0 l }

fun testMap l = { map addOne l }

fun main = {
  sum (testMap list)
}

// CHECK: Result: 3
