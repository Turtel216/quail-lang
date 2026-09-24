// RUN: not %qc %s -o %t 2>&1 | FileCheck %s

// A class says an instance of it is already an instance of each of its
// superclasses. That is only answered when the dictionary is laid out, since
// the answer is one of its fields.

class Same a = { fun same (x: a) (y: a) : Bool }
class Same a => Ranked a = { fun below (x: a) (y: a) : Bool }

instance Ranked Int = { fun below x y = { x <= y } }

fun main = { 1 }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the instance Ranked Int needs Same Int, which no instance provides
