// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun sum (xs: List Int) : Int = { foldr (\a b -> { a + b }) 0 xs }

fun scaleAll (f: Int -> Int) (xs: List Int) : List Int = { map f xs }

fun main = { [1, 2, 3] |> scaleAll (\x -> { x * 7 }) |> sum }

// CHECK: Result: 42
