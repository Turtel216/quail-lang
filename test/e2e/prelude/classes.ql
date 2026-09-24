// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// The three classes section 0 asked for, used through the operators that
// stand for their methods, at every type the prelude gives an instance for.

fun Eq a => allSame (x: a) (xs: List a) : Bool = {
    match xs with {
        Nil -> { True }
        Cons y ys -> { if x == y { allSame x ys } else { False } }
    }
}

fun Ord a => biggest (x: a) (y: a) : a = { if x >= y { x } else { y } }

fun Num a => twice (x: a) : a = { x + x }

fun main = {
    let {
        fun onInt = { if 3 == 3 { if 3 != 4 { 1 } else { 0 } } else { 0 } }
        fun onBool = { if True == True { if False < True { 2 } else { 0 } } else { 0 } }
        fun onList = { if [1, 2] == [1, 2] { if [1] < [1, 2] { 4 } else { 0 } } else { 0 } }
        fun onMaybe = { if Just 5 == Just 5 { if Nothing != Just 5 { 8 } else { 0 } } else { 0 } }
        fun ordered = { biggest 9 7 }
        fun overloaded = { twice 9 - allSameToNumber }
        fun allSameToNumber = { if allSame 1 [1, 1] { 0 } else { 100 } }
    } in {
        onInt + onBool + onList + onMaybe + ordered + overloaded
    }
}

// CHECK: Result: 42
