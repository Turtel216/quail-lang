fun list = { Cons 1 (Cons 1 (Cons 1 (Cons 1 Nil))) }

fun add x y = { x + y }

fun main = { foldr add 0 list }
