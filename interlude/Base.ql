type Option a = { 
    None, 
    Some a 
}

type Bool = { True , False}

type List a = { 
    Nil, 
    Cons a (List a) 
}

fun map f l = {
    match lst with {
        Nil -> { Nil }
        Cons x xs -> { Cons  (f x) (map f xs) }
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
        Cons x xs -> { f x (foldr f b xs)}
    }
}

fun main = { 0 }
