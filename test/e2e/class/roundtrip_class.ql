// RUN: %qc %s --dump-source > %t.printed.ql
// RUN: %qc %s --dump-ast > %t.before
// RUN: %qc %t.printed.ql --dump-ast > %t.after
// RUN: diff %t.before %t.after

// A class declaration survives being written back out: no superclass, one
// superclass, and several, with methods that do and do not carry a default.

class Eq a = {
    fun eq (x: a) (y: a) : Bool
    fun neq (x: a) (y: a) : Bool = { not (eq x y) }
}

class Eq a => Ord a = {
    fun le (x: a) (y: a) : Bool
    fun lt (x: a) (y: a) : Bool = { if le x y { neq x y } else { False } }
}

class (Eq a, Ord a) => Bounded a = {
    fun smallest : a
    fun largest : a
}
