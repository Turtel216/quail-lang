// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun double x = { x * 2 }
fun inc x = { x + 1 }
fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

// A composed function is an ordinary function, so a pipe may hand a value
// to one, and one may be built out of a partial application.
fun main = { [1, 2, 3] |> map (inc . double) |> sum . map inc }

// CHECK: Result: 18
