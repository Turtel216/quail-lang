// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A default is compiled once and handed the dictionary it is a field of, so
// it can call any other method of its class. An instance that writes its own
// gets that instead.

class Same a = {
    fun same (x: a) (y: a) : Bool
    fun differs (x: a) (y: a) : Bool = { not (same x y) }
}

instance Same Int = { fun same x y = { x == y } }

instance Same Bool = {
    fun same x y = { match x with { True -> { y } False -> { not y } } }
    fun differs x y = { match x with { True -> { not y } False -> { y } } }
}

fun main = {
    if differs 1 2 {
        if differs True True { 0 } else { if differs True False { 42 } else { 0 } }
    } else { 0 }
}

// CHECK: Result: 42
