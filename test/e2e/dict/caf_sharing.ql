// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// An instance that holds under nothing builds one dictionary. Mentioning it
// in several places, and reaching it once through another instance, is the
// same node every time.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Same a => Same (List a) = { fun same xs ys = { True } }

fun a = { same 1 1 }
fun b = { same 2 2 }
fun c = { same [3] [3] }

fun main = { if a { if b { if c { 42 } else { 0 } } else { 0 } } else { 0 } }

// CHECK: Result: 42
