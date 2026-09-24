// RUN: not %qc %s --check-classes 2>&1 | FileCheck %s

type Bool = { True, False }

class C a = { fun Eq b => f (x: a) : Bool }

// CHECK: the method f writes a context of its own
