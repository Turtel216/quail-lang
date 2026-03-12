fun list = { Cons 1 (Cons 2 (Cons 3 (Cons 4 Nil))) }

fun add x y = { x + y }
fun sum l = { foldr add 0 l }

fun skipAdd x y = { y + 1 }
fun length l = { foldr skipAdd 0 l }

fun main = { sum list + length list }
