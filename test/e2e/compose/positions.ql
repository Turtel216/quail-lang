// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun inc x = { x + 1 }
fun double x = { x * 2 }
fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

// A composition may stand wherever an expression may: as the function a
// list literal is mapped with, as what a match takes apart, and in a branch.
fun main = {
    match map (double . inc) [6, 5] with {
        Nil -> { 0 }
        Cons x xs -> { [(double . inc) x, sum xs] |> sum }
    }
}

// CHECK: Result: 42
