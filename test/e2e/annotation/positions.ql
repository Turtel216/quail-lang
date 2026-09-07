// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

type Box a = { Box a }

// An annotation may name a built-in type, a declared one, a function type,
// or a type variable, and stand on a parameter, on the return, or on both.
fun unbox (b: Box Int) : Int = { match b with { Box v -> { v } } }

fun keep (x: a) : a = { x }

fun pair (f: Int -> Int) (g: Int -> Int) = { f . g }

fun run (f: Int -> Int) x : Int = { f x }

fun answer : Int = { 21 }

fun main = {
    let {
        fun stay (n: Int) : Int = { keep n }
    } in {
        run (pair (\n -> { n + 0 }) stay) (unbox (Box answer)) * 2
    }
}

// CHECK: Result: 42
