// RUN: %qc %s --dump-types | FileCheck %s

// A context the program writes is what the definition holds under, so long
// as the body needs no more than it. A superclass counts as provided.

class Same a = { fun same (x: a) (y: a) : Bool }
class Same a => Ranked a = { fun below (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Ranked Int = { fun below x y = { x <= y } }

fun Same a => same (x: a) (y: a) : Bool = { same x y }

fun Ranked a => viaSuper (x: a) (y: a) : Bool = { same x y }

fun (Same a, Ranked a) => both (x: a) (y: a) : Bool = { below x y }

// CHECK-DAG: same : Same a => a -> a -> Bool
// CHECK-DAG: viaSuper : Ranked a => a -> a -> Bool
// CHECK-DAG: both : (Ranked a, Same a) => a -> a -> Bool
