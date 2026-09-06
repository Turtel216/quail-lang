// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun above n x = { x > n }

fun toInt b = { if b { 1 } else { 0 } }

fun main = { 5 |> above 3 |> toInt }

// CHECK: Result: 1
