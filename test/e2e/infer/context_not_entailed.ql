// RUN: not %qc %s --dump-types 2>&1 | FileCheck %s

// The body needs more than the definition promised. Ranked implies Same, but not
// the other way round.

class Same a = { fun same (x: a) (y: a) : Bool }
class Same a => Ranked a = { fun below (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }
instance Ranked Int = { fun below x y = { x <= y } }

fun Same a => bad (x: a) (y: a) : Bool = { below x y }

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the body needs Ranked a, which the declared context Same a does not provide
