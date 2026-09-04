// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

// The right side may be any expression, including a lambda that captures.
fun weigh n l = { l |> map (\x -> { n * x }) |> sum }

fun main = { [1, 2, 3] |> weigh 7 }

// CHECK: Result: 42
