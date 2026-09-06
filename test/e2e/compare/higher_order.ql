// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun toInt b = { if b { 1 } else { 0 } }

fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

fun main = { [1, 5, 2, 8] |> map (\x -> { toInt (x > 2) }) |> sum }

// CHECK: Result: 2
