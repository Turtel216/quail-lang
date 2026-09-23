// RUN: not %qc %s --dump-types 2>&1 | FileCheck %s

// An instance method may only assume what its instance holds under. This one
// needs Show for the element type, and the instance only asks for Same.

class Same a = { fun same (x: a) (y: a) : Bool }
class Show a = { fun show (x: a) : Int }

instance Same Int = { fun same x y = { x == y } }

instance Same a => Same (List a) = {
    fun same xs ys = {
        match xs with {
            Nil -> { True }
            Cons x rest -> { same (show x) 1 }
        }
    }
}

// CHECK: an error occured while checking the types of the program
// CHECK-SAME: the method same needs Show a, which the instance does not hold under
