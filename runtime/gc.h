#ifndef QUAIL_RT_GC_H_
#define QUAIL_RT_GC_H_

#include <stddef.h>

#include "node.h"
#include "vec.h"

struct gmachine;

/* =========================================================================
 * Garbage collection.
 * =========================================================================
 *
 * Two generations, as in OCaml:
 *
 *   Minor GC   Cheney stop-and-copy over the bump-allocated minor heap.
 *              Survivors are promoted to the major heap.  Runs to
 *              completion; it is bounded by the live set, not the heap size.
 *
 *   Major GC   Incremental tri-color mark-and-sweep over the promoted
 *              objects, which are individually malloc'd and threaded onto an
 *              intrusive list.  Runs in bounded slices, one per minor GC, so
 *              a collection never introduces a long pause.
 *
 * Because the major collector is interleaved with the mutator, two barriers
 * keep its invariants intact between slices; both live in gc_write_barrier.
 * ========================================================================= */

enum gc_phase { GC_IDLE = 0, GC_MARK = 1, GC_SWEEP = 2 };

struct gc_state {
    enum gc_phase phase;

    /* Mark phase: the grey set, as an explicit worklist. */
    struct node_vec mark_stack;

    /* Sweep phase: cursor into the major heap list.  A pointer to the link
     * field lets a dead node be unlinked without a previous-node variable. */
    struct node_base **sweep_ptr;
};

void gc_state_init(struct gc_state *s);
void gc_state_free(struct gc_state *s);

/* Promote every live minor-heap object to the major heap and empty the minor
 * heap.  Also advances the major collector by one slice. */
void minor_gc(struct gmachine *g);

/* Run a complete major mark-and-sweep cycle, finishing any cycle already in
 * progress.  Intended for shutdown and debugging; the steady state uses the
 * incremental slices driven by minor_gc. */
void gmachine_gc(struct gmachine *g);

/* Record that `obj`'s children have just been overwritten.  `old_color` is
 * the color `obj` had before the write.  Must be called after every mutation
 * of a heap node's pointer fields. */
void gc_write_barrier(struct gmachine *g, struct node_base *obj,
                      enum gc_color old_color);

/* Release every major-heap object.  Only valid at shutdown. */
void gc_free_all(struct gmachine *g);

#endif /* QUAIL_RT_GC_H_ */
