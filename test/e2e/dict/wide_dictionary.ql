// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

// A dictionary is an ordinary data node, whose fields live in an array of
// their own rather than in the node. Nothing about it is bounded by what a
// node header can say, so a class may declare as many methods as it likes.

class Wide a = {
    fun ma (x: a) : Int
    fun mb (x: a) : Int
    fun mc (x: a) : Int
    fun md (x: a) : Int
    fun me (x: a) : Int
    fun mf (x: a) : Int
    fun mg (x: a) : Int
    fun mh (x: a) : Int
    fun mi (x: a) : Int
    fun mj (x: a) : Int
    fun mk (x: a) : Int
    fun ml (x: a) : Int
    fun mm (x: a) : Int
    fun mn (x: a) : Int
    fun mo (x: a) : Int
    fun mp (x: a) : Int
    fun mq (x: a) : Int
    fun mr (x: a) : Int
    fun ms (x: a) : Int
    fun mt (x: a) : Int
}

instance Wide Int = {
    fun ma x = { 1 }
    fun mb x = { 2 }
    fun mc x = { 3 }
    fun md x = { 4 }
    fun me x = { 5 }
    fun mf x = { 6 }
    fun mg x = { 7 }
    fun mh x = { 8 }
    fun mi x = { 9 }
    fun mj x = { 10 }
    fun mk x = { 11 }
    fun ml x = { 12 }
    fun mm x = { 13 }
    fun mn x = { 14 }
    fun mo x = { 15 }
    fun mp x = { 16 }
    fun mq x = { 17 }
    fun mr x = { 18 }
    fun ms x = { 19 }
    fun mt x = { 20 }
}

fun main = { mt 0 + ms 0 + mc 0 }

// CHECK: Result: 42
