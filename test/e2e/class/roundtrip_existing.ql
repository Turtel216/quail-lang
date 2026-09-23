// RUN: %qc %s --dump-source > %t.printed.ql
// RUN: %qc %s --dump-ast > %t.before
// RUN: %qc %t.printed.ql --dump-ast > %t.after
// RUN: diff %t.before %t.after

// Everything that could already be written survives the same round trip, so
// that the printer is not only right about what type classes added.

type Tree a = { Leaf, Node a (Tree a) (Tree a) }

type Wrap a b = { MkWrap (a -> b) (List a) }

fun apply (f: Int -> Int) (x: Int) : Int = { f (f x) }

fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

fun classify b c = { [if b { 1 } else { if c { 2 } else { 3 } }] }

fun mixed l = {
    let {
        fun double x = { x * 2 }
        fun total xs = {
            match xs with {
                Nil -> { 0 }
                Cons x rest -> { double x + total rest }
            }
        }
    } in {
        l |> map double |> total
    }
}

fun composed = { sum . map (\x -> { x - 1 }) }

fun compares x y = { if x <= y { x != y } else { x >= y } }

fun divides x y = { x / y }
