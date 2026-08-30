// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

fun list = { Cons 1 (Cons 2 (Cons 3 Nil)) }

fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

// The lambda captures n, so it is lifted with n as an extra parameter.
fun addToAll n l = { map (\x -> { n + x }) l }

fun main = { sum (addToAll 12 list) }

// CHECK: Result: 42
