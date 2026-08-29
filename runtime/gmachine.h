#ifndef QUAIL_RT_GMACHINE_H_
#define QUAIL_RT_GMACHINE_H_

#include <stddef.h>
#include <stdint.h>

#include "gc.h"
#include "heap.h"
#include "node.h"
#include "stack.h"
#include "vec.h"

/* =========================================================================
 * G-machine state.
 * =========================================================================
 *
 * One instance per program, threaded through every compiled supercombinator.
 *
 * `stack` must stay the first member: generated code derives the
 * `struct stack *` it passes to the stack routines by taking the address of
 * field 0 (see CodeGenerator::unwrapGmachineStackPtr).
 * ========================================================================= */

struct gmachine {
    struct stack stack;
    struct minor_heap minor_heap;

    /* Major-heap objects mutated to point into the minor heap.  Extra roots
     * for the next minor GC; cleared by it. */
    struct node_vec remembered_set;

    struct gc_state gc_state;

    /* Major heap: an intrusive list of promoted objects, and the object
     * count at which the next mark-and-sweep cycle starts. */
    struct node_base *gc_nodes;
    size_t gc_node_count;
    size_t gc_node_threshold;
};

void gmachine_init(struct gmachine *g);
void gmachine_free(struct gmachine *g);

/* -- G-machine instructions ------------------------------------------------
 * Called by generated code; see the Instruction subclasses in the compiler. */

/* Slide: drop `n` values beneath the top of the stack. */
void gmachine_slide(struct gmachine *g, size_t n);

/* Update: overwrite the redex `o` slots below the top with an indirection to
 * the value on top, then pop that value. */
void gmachine_update(struct gmachine *g, size_t o);

/* Alloc: push `o` placeholder indirections, to be filled in by Update. */
void gmachine_alloc(struct gmachine *g, size_t o);

/* Pack: replace the top `n` values with a data node carrying tag `t`. */
void gmachine_pack(struct gmachine *g, size_t n, int8_t t);

/* Split: replace the data node on top with its first `n` fields. */
void gmachine_split(struct gmachine *g, size_t n);

#endif /* QUAIL_RT_GMACHINE_H_ */
