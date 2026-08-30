// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun list = { Cons 1 (Cons 2 (Cons 3 Nil)) }

fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

// A lambda whose body is a let, both sharing the captured n.
fun weight n l = { map (\x -> { let { fun w = { n * x } } in { w + x } }) l }

fun main = { sum (weight 6 list) }

// CHECK: Result: 42
