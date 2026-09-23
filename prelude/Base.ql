type Maybe a = {
    Nothing,
    Just a
}

type Bool = { True , False}

fun not b = {
    match b with {
          True -> { False }
          False -> { True }
    }
}

class Eq a = {
    fun eq (x: a) (y: a) : Bool
    fun neq (x: a) (y: a) : Bool = { not (eq x y) }
}

class Eq a => Ord a = {
    fun lt (x: a) (y: a) : Bool
    fun le (x: a) (y: a) : Bool = { if lt x y { True } else { eq x y } }
    fun gt (x: a) (y: a) : Bool = { lt y x }
    fun ge (x: a) (y: a) : Bool = { le y x }
}

class Num a = {
    fun add (x: a) (y: a) : a
    fun sub (x: a) (y: a) : a
    fun mul (x: a) (y: a) : a
    fun div (x: a) (y: a) : a
    fun fromInt (n: Int) : a
}

instance Eq Int = {
    fun eq x y = { primEq x y }
    fun neq x y = { primNeq x y }
}

instance Ord Int = {
    fun lt x y = { primLt x y }
    fun le x y = { primLe x y }
    fun gt x y = { primGt x y }
    fun ge x y = { primGe x y }
}

instance Num Int = {
    fun add x y = { primAdd x y }
    fun sub x y = { primSub x y }
    fun mul x y = { primMul x y }
    fun div x y = { primDiv x y }
    fun fromInt n = { n }
}

instance Eq Bool = {
    fun eq x y = { match x with { True -> { y } False -> { not y } } }
}

instance Ord Bool = {
    fun lt x y = { match x with { True -> { False } False -> { y } } }
}

instance Eq a => Eq (List a) = {
    fun eq xs ys = {
        match xs with {
            Nil -> { match ys with { Nil -> { True } Cons y rest -> { False } } }
            Cons x more -> {
                match ys with {
                    Nil -> { False }
                    Cons y rest -> { if eq x y { eq more rest } else { False } }
                }
            }
        }
    }
}

instance Ord a => Ord (List a) = {
    fun lt xs ys = {
        match xs with {
            Nil -> { match ys with { Nil -> { False } Cons y rest -> { True } } }
            Cons x more -> {
                match ys with {
                    Nil -> { False }
                    Cons y rest -> {
                        if lt x y { True } else {
                            if eq x y { lt more rest } else { False }
                        }
                    }
                }
            }
        }
    }
}

instance Eq a => Eq (Maybe a) = {
    fun eq m n = {
        match m with {
            Nothing -> { match n with { Nothing -> { True } Just y -> { False } } }
            Just x -> {
                match n with { Nothing -> { False } Just y -> { eq x y } }
            }
        }
    }
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
