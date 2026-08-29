#ifndef RUNTIME_H_
#define RUNTIME_H_

/* =========================================================================
 * Quail runtime -- public interface.
 * =========================================================================
 *
 * The runtime implements lazy graph reduction for compiled Quail programs:
 * a G-machine over a two-generation garbage-collected heap.  It is compiled
 * and linked against the generated object file by ff::drv::linkToRuntime.
 *
 * Everything the generated code calls is declared here.  The declarations
 * must stay in step with CodeGenerator::createFunctions, which redeclares
 * these signatures in LLVM IR; two of them are load-bearing beyond their
 * prototypes:
 *
 *   - `header` is field 0 of every node, and the generated code loads the
 *     data constructor tag straight out of it (unwrapDataTag).
 *   - `stack` is field 0 of `struct gmachine`, so the generated code can
 *     derive a `struct stack *` from a `struct gmachine *` by taking the
 *     address of field 0 (unwrapGmachineStackPtr).
 *
 * Integers are unboxed and never allocated: the generated code tags and
 * untags them inline (createNum / unwrapNum), matching node.h.
 *
 * Layering, low to high:
 *
 *   panic.h     abort paths for unrecoverable failures
 *   node.h      value tagging, object header, node layouts, child traversal
 *   vec.h       growable array of tagged values, shared by stack and both GCs
 *   stack.h     G-machine stack, and the collectors' root set
 *   heap.h      minor heap bump allocator and the node constructors
 *   gc.h        minor copying GC and incremental major mark-and-sweep
 *   gmachine.h  machine state, lifecycle, and the G-machine instructions
 *   eval.h      graph reduction and result printing
 * ========================================================================= */

#include "eval.h"
#include "gc.h"
#include "gmachine.h"
#include "heap.h"
#include "node.h"
#include "panic.h"
#include "stack.h"
#include "vec.h"

#endif /* RUNTIME_H_ */
