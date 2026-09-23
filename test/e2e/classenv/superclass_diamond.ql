// RUN: %qc %s --check-classes | FileCheck %s

// Two paths to the same superclass are not a cycle. Reaching a class twice
// has to be told apart from reaching it while it is still being walked.

type Bool = { True, False }

class A a = { fun f (x: a) : Bool }
class A a => B a = { fun g (x: a) : Bool }
class A a => C a = { fun h (x: a) : Bool }
class (B a, C a) => D a = { fun i (x: a) : Bool }

// CHECK: CLASS A a
// CHECK: CLASS B a
// CHECK-NEXT: SUPER A a
// CHECK: CLASS C a
// CHECK-NEXT: SUPER A a
// CHECK: CLASS D a
// CHECK-NEXT: SUPER B a
// CHECK-NEXT: SUPER C a
