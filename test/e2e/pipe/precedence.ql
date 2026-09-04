// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun add x y = { x + y }
fun double x = { x * 2 }

// A pipe binds looser than the arithmetic operators and than application,
// so this is add 20 (double (1 + 10)).
fun main = { 1 + 10 |> double |> add 20 }

// CHECK: Result: 42
