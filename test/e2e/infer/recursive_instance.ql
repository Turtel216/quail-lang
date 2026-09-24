// RUN: %qc %s --dump-types | FileCheck %s

// An instance method calling the method it implements goes through the class
// like any other use, so the constraint it wants is Same (List a), which
// reduces to the Same a the instance already holds under.

class Same a = { fun same (x: a) (y: a) : Bool }

instance Same Int = { fun same x y = { x == y } }

instance Same a => Same (List a) = {
    fun same xs ys = {
        match xs with {
            Nil -> { True }
            Cons x rest -> {
                match ys with {
                    Nil -> { False }
                    Cons y more -> { if same x y { same rest more } else { False } }
                }
            }
        }
    }
}

fun deep = { same [[1], [2]] [[1], [2]] }

// CHECK: deep : Bool
