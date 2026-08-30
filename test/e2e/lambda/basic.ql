// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun list = { Cons 1 (Cons 2 (Cons 3 Nil)) }

fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

fun main = { sum (map (\x -> { x * 7 }) list) }

// CHECK: Result: 42
