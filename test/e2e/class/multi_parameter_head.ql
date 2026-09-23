// RUN: %qc %s --dump-ast | FileCheck %s

// A head over more than one argument parses. Whether a class may have more
// than one parameter is a question about the class, not about the syntax, so
// it is the class environment that answers it.

class Convert a b = {
    fun convert (x: a) : b
}

instance Convert Int Bool = {
    fun convert x = { True }
}

// CHECK: CLASS: Convert a b
// CHECK: INSTANCE: Convert Int Bool
