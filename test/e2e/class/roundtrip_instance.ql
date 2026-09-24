// RUN: %qc %s --dump-source > %t.printed.ql
// RUN: %qc %s --dump-ast > %t.before
// RUN: %qc %t.printed.ql --dump-ast > %t.after
// RUN: diff %t.before %t.after

// Instance declarations survive being written back out, with an empty
// context, one predicate, and several, over both a bare type and an applied
// one.

instance Eq Int = {
    fun eq x y = { primIntEq x y }
}

instance Eq a => Eq (List a) = {
    fun eq xs ys = {
        match xs with {
            Nil -> { True }
            Cons x rest -> { False }
        }
    }
}

instance (Eq a, Eq b) => Eq (Pair a b) = {
    fun eq p q = { True }
    fun neq p q = { False }
}
