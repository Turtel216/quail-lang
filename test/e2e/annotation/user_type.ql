// RUN: %qc %s -o %t
// RUN: %t | FileCheck %s

type Tree = {
    Leaf,
    Node Tree Tree
}

// The type is declared below the function that names it; every data type in
// a scope is in scope for all of them.
fun count (t: Tree) : Int = {
    match t with {
        Leaf -> { 1 }
        Node l r -> { count l + count r }
    }
}

fun unwrap (m: Maybe Int) (fallback: Int) : Int = {
    match m with {
        Nothing -> { fallback }
        Just v -> { v }
    }
}

fun main = { count (Node Leaf Leaf) * unwrap (Just 21) 0 }

// CHECK: Result: 42
