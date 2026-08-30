// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// The inner lambda captures both n and x, the outer only n.
fun addThree n = { \x -> { \y -> { n + x + y } } }

fun compose f g = { \x -> { f (g x) } }

fun double x = { x * 2 }

fun main = { compose (addThree 10 12) double 10 }

// CHECK: Result: 42
