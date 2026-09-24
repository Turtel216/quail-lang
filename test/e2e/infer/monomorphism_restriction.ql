// RUN: %qc %s --dump-types | FileCheck %s

// A definition that takes no arguments is a value, computed once, so its
// constrained variables are not generalized and get settled instead. One
// that takes an argument, or writes its context, is generalized as usual.
//
// A written number is where the constraint comes from: it stands for
// whatever type it is used at, which is what Num says.

fun theOne = { 1 }

fun make n = { n + 1 }

fun Num a => declared : a = { 1 }

// CHECK-DAG: theOne : Int
// CHECK-DAG: make : Num a => a -> a
// CHECK-DAG: declared : Num a => a
