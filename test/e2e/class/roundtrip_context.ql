// RUN: %qc %s --dump-source > %t.printed.ql
// RUN: %qc %s --dump-ast > %t.before
// RUN: %qc %t.printed.ql --dump-ast > %t.after
// RUN: diff %t.before %t.after

// A context on a definition survives, written bare or in parentheses, with
// and without the annotations it constrains.

fun Eq a => member (x: a) (xs: List a) : Bool = {
    match xs with {
        Nil -> { False }
        Cons y ys -> { if eq x y { True } else { member x ys } }
    }
}

fun (Eq a) => same x y = { eq x y }

fun (Eq a, Ord b) => rank (x: a) (y: b) : Int = { 1 }

fun (Eq (List a)) => nested (xs: List a) : Bool = { eq xs xs }
