// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A class may declare no methods of its own and only gather a superclass.
// Its dictionary is then one field, and the instance that builds it takes
// nothing.

class Same a = { fun same (x: a) (y: a) : Bool }
class Same a => Sorted a = { }

instance Same Int = { fun same x y = { x == y } }
instance Sorted Int = { }

fun Sorted a => check (x: a) (y: a) : Bool = { same x y }

fun main = { if check 1 1 { 42 } else { 0 } }

// CHECK: Result: 42
