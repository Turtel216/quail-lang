type List a = { Nil, Cons a (List a) }

fun map f l = {
    match l with {
        Nil -> { Nil }
        Cons x xs -> { Cons (f x) (map f xs) }
    }
}

fun foldl f b l = {
    match l with {
        Nil -> { b }
        Cons x xs -> { foldl f (f b x) xs }
    }
}

fun foldr f b l = {
    match l with {
        Nil -> { b }
        Cons x xs -> { f x (foldr f b xs) }
    }
}

fun list = { Cons 1 (Cons 2 (Cons 3 (Cons 4 Nil))) }

fun add x y = { x + y }
fun sum l = { foldr add 0 l }

fun skipAdd x y = { y + 1 }
fun length l = { foldr skipAdd 0 l }

fun main = { sum list + length list }
