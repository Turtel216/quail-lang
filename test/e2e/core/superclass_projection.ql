// RUN: %qc %s --dump-core | FileCheck %s

// A superclass is taken out of the dictionary already in hand, not solved
// again: the Same the body needs comes from the Ranked it was handed.

class Same a = { fun same (x: a) (y: a) : Bool }
class Same a => Ranked a = { fun below (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Ranked Int = { fun below x y = { x <= y } }

fun viaSuper x y = { if below x y { same x y } else { False } }

// CHECK: DEFN: viaSuper
// CHECK-NEXT: PARAM: d$Ranked$0
// CHECK: LID: below
// CHECK-NEXT: DICT: d$Ranked$0
// CHECK: LID: same
// CHECK-NEXT: DICT: (Ranked$super$Same d$Ranked$0)
