// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

type Pair a b = { MkPair a b }

// A lowercase name in a signature stands for whatever fits, and belongs to
// the definition that wrote it: these two share no type by sharing an `a`.
fun first (p: Pair a b) : a = { match p with { MkPair l r -> { l } } }

fun same (x: a) : a = { x }

fun main = { first (MkPair 42 True) + same 0 }

// CHECK: Result: 42
