// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// An instance method may take fewer arguments than its class signature
// mentions, so long as what it hands back takes the rest.

type Colour = { Red, Green }

class Same a = { fun same (x: a) (y: a) : Bool }

fun sameInt x y = { x == y }

instance Same Int = { fun same = { sameInt } }

instance Same Colour = {
    fun same x y = {
        match x with {
            Red -> { match y with { Red -> { True } Green -> { False } } }
            Green -> { match y with { Red -> { False } Green -> { True } } }
        }
    }
}

fun main = {
    if same 20 20 { if same Red Green { 0 } else { 42 } } else { 0 }
}

// CHECK: Result: 42
