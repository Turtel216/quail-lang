type Maybe a = { 
    Nothing, 
    Just a 
}

type Bool = { True , False}

fun not b = {
    match b with {
          True -> { False }
          False -> { False }
    }
}

fun if c t e = {
    match c with {
          True -> { t }
          False -> { e }
    }
}

type List a = { 
    Nil, 
    Cons a (List a) 
}

fun map f l = {
    match l with {
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

fun head l = {
    match l with {
        Nil -> { Nothing }
        Cons x xs -> { Just x }
    }
}
