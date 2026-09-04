// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }
fun double x = { x * 2 }

fun sum l = { foldr add 0 l }

// Pipes associate to the left, so the list is threaded front to back.
fun main = { [1, 2, 3, 4] |> map double |> sum |> add 22 }

// CHECK: Result: 42
