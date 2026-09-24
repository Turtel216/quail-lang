// RUN: %qc %s --dump-types | FileCheck %s

// A constraint a superclass already implies is not carried twice: holding
// Ranked a is holding Same a, so the inferred context keeps only the first.

class Same a = { fun same (x: a) (y: a) : Bool }
class Same a => Ranked a = { fun below (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Ranked Int = { fun below x y = { x <= y } }

fun sorted x y = { if below x y { same x y } else { False } }

// CHECK: sorted : Ranked a => a -> a -> Bool
