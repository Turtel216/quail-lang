// RUN: %qc %s --dump-core | FileCheck %s

// The dictionaries a definition takes are in one fixed order, by class name
// and then by the type constrained, whatever order the program wrote them
// in. A use and the definition it applies itself to agree without either
// having to look at the other.

class Same a = { fun same (x: a) (y: a) : Bool }
class Ranked a = { fun below (x: a) (y: a) : Bool }
class Show a = { fun show (x: a) : Int }

instance Same Int = { fun same x y = { x == y } }
instance Ranked Int = { fun below x y = { x <= y } }
instance Show Int = { fun show x = { x } }

fun (Show a, Same a, Ranked a) => declared (x: a) : Int = {
    if below x x { if same x x { show x } else { 0 } } else { 1 }
}

fun inferred x = { if below x x { if same x x { show x } else { 0 } } else { 1 } }

// CHECK: DEFN: declared
// CHECK: PARAM: d$Ranked$0
// CHECK-NEXT: PARAM: d$Same$1
// CHECK-NEXT: PARAM: d$Show$2
// CHECK: DEFN: inferred
// CHECK-NEXT: PARAM: d$Ranked$0
// CHECK-NEXT: PARAM: d$Same$1
// CHECK-NEXT: PARAM: d$Show$2
