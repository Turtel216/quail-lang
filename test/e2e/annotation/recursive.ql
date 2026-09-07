// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A definition sees its own declared type, so recursion and mutual
// recursion are checked against the signature rather than around it.
fun length (xs: List a) : Int = {
    match xs with {
        Nil -> { 0 }
        Cons y ys -> { 1 + length ys }
    }
}

fun isEven (n: Int) : Bool = { if n == 0 { True } else { isOdd (n - 1) } }

fun isOdd (n: Int) : Bool = { if n == 0 { False } else { isEven (n - 1) } }

fun adder (n: Int) : Int -> Int = { \m -> { n + m } }

fun main = {
    if isEven (length [1, 2] + length [True, False]) {
        adder 38 (length [Nothing, Just 1, Nothing, Just 2])
    } else {
        0
    }
}

// CHECK: Result: 42
