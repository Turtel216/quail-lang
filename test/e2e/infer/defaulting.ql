// RUN: %qc %s --dump-types | FileCheck %s

// A variable nothing that uses the definition can pin down is settled by the
// defaulting rules, so long as one of its constraints is the numeric class
// and the type they settle on satisfies the rest. Num is the prelude's, so
// this is the same class the rules are written about.

class Shown a = { fun render (x: a) : Int }

instance Shown Int = { fun render x = { x } }

fun showNum n = { render (fromInt n) }

// CHECK: showNum : Int -> Int
