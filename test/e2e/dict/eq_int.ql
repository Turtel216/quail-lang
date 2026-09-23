// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// The smallest thing dictionary passing has to do: a class, a ground
// instance, and a use that picks it.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }

fun main = { if same 3 3 { if same 3 4 { 0 } else { 42 } } else { 0 } }

// CHECK: Result: 42
