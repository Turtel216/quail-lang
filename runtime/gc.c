#include "gc.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "gmachine.h"
#include "heap.h"
#include "panic.h"
#include "stack.h"

/* Work done per incremental major GC slice, in objects. */
enum { GC_MARK_SLICE = 256, GC_SWEEP_SLICE = 256 };

/* Smallest major heap the collector will bother shrinking the trigger to. */
enum { GC_MIN_THRESHOLD = 128 };

/* True for a value the next minor GC will have to relocate.  The heap-pointer
 * test must come first: a tagged integer is not an address, but nothing stops
 * its bit pattern from falling inside the minor heap's range. */
static inline int gc_is_minor_ptr(const struct gmachine *g,
                                  const struct node_base *v) {
    return node_is_heap_ptr(v) && minor_heap_contains(&g->minor_heap, v);
}

/* =========================================================================
 * Major heap bookkeeping
 * ========================================================================= */

/* Take ownership of `n` on the major heap: thread it onto the sweep list and
 * count it toward the next cycle's trigger. */
static void major_adopt(struct gmachine *g, struct node_base *n) {
    n->gc_next = g->gc_nodes;
    g->gc_nodes = n;
    g->gc_node_count++;
}

/* Release a major-heap object and everything it owns. */
static void major_release(struct node_base *n) {
    node_free_resources(n);
    free(n);
}

void gc_free_all(struct gmachine *g) {
    struct node_base *n = g->gc_nodes;

    while (n != NULL) {
        struct node_base *next = n->gc_next;
        major_release(n);
        n = next;
    }

    g->gc_nodes = NULL;
    g->gc_node_count = 0;
}

/* =========================================================================
 * Minor GC -- Cheney stop-and-copy
 * =========================================================================
 *
 * A single breadth-first pass over the live graph, with no recursion:
 *
 *   evacuate  copy one object to the major heap, leave a forwarding address
 *             behind, and append the copy to the scan queue.
 *   scavenge  update one evacuated object's child slots, evacuating any that
 *             still point into the minor heap.
 *
 * The scan queue plays the role of Cheney's to-space scan pointer.  Promoted
 * objects are malloc'd individually rather than laid out contiguously, so a
 * linear scan pointer is not available; an explicit FIFO gives the same
 * single-pass O(live) behaviour.
 * ========================================================================= */

/* Copy `n` to the major heap if it still lives in the minor heap.  Returns
 * the value that should replace it: unchanged for integers, NULL, and
 * already-promoted objects; the forwarding address for objects copied
 * earlier in this collection. */
static struct node_base *evacuate(struct gmachine *g, struct node_base *n,
                                  struct node_vec *queue) {
    if (!gc_is_minor_ptr(g, n)) {
        return n;
    }
    if (hdr_is_fwd(n->header)) {
        return hdr_fwd(n->header);
    }

    size_t size = node_size_bytes(n);
    assert(size > 0 && "evacuating an object with no header");

    struct node_base *copy = malloc(size);
    if (copy == NULL) {
        rt_oom("promoted object");
    }
    memcpy(copy, n, size);
    major_adopt(g, copy);

    /* Leave a forwarding address so other references to `n` found later in
     * this collection resolve to the copy.  The low header bits survive, so
     * the linear heap walk below can still read the object's size. */
    n->header = hdr_with_fwd(n->header, copy);

    node_vec_push(queue, copy);
    return copy;
}

/* Update every child slot of `node`, evacuating minor-heap targets. */
static void scavenge(struct gmachine *g, struct node_base *node,
                     struct node_vec *queue) {
    struct node_child_iter it;
    struct node_base **slot;

    node_children_begin(&it, node);
    while ((slot = node_children_next(&it)) != NULL) {
        *slot = evacuate(g, *slot, queue);
    }
}

/* Forward declarations: minor GC hands promoted objects to the major
 * collector, and drives its incremental slices. */
static void gc_darken_children(struct gmachine *g, struct node_base *node);
static void gc_start_cycle(struct gmachine *g);
static void gc_slice(struct gmachine *g);

/* Reclaim resources owned by objects that did not survive.  A promoted
 * object's resources now belong to its copy, so only non-forwarded objects
 * are released here. */
static void minor_heap_reclaim_dead(struct minor_heap *h) {
    for (struct node_base *n = minor_heap_first(h); n != NULL;
         n = minor_heap_next(h, n)) {
        assert(node_size_bytes(n) > 0 && "unheadered object in minor heap");

        if (!hdr_is_fwd(n->header)) {
            node_free_resources(n);
        }
    }
}

/* An object promoted mid-cycle has not been traced, and the mutator may hold
 * the only reference to it.  Publish it as reachable, and make sure its
 * major-heap children survive too. */
static void gc_adopt_promoted(struct gmachine *g, struct node_vec *promoted) {
    for (size_t i = 0; i < promoted->count; i++) {
        struct node_base *obj = promoted->data[i];
        node_set_color(obj, GC_BLACK);
        gc_darken_children(g, obj);
    }
}

void minor_gc(struct gmachine *g) {
    struct node_vec queue;
    node_vec_init(&queue, 0);

    /* Roots: every stack slot that points into the minor heap. */
    struct node_base **roots = stack_slots(&g->stack);
    for (size_t i = 0; i < stack_count(&g->stack); i++) {
        roots[i] = evacuate(g, roots[i], &queue);
    }

    /* Additional roots: major-heap objects the mutator pointed back into the
     * minor heap since the last collection. */
    for (size_t i = 0; i < g->remembered_set.count; i++) {
        scavenge(g, g->remembered_set.data[i], &queue);
    }
    node_vec_clear(&g->remembered_set); /* every cross-generation ref resolved */

    /* Cheney scan loop.  Scavenging appends newly evacuated objects to the
     * queue, so this terminates once the live graph is exhausted. */
    for (size_t scan = 0; scan < queue.count; scan++) {
        scavenge(g, queue.data[scan], &queue);
    }

    if (g->gc_state.phase != GC_IDLE) {
        gc_adopt_promoted(g, &queue);
    }

    node_vec_free(&queue);

    minor_heap_reclaim_dead(&g->minor_heap);
    minor_heap_reset(&g->minor_heap);

    /* Pay for the promotions: start a major cycle if the heap has grown past
     * the trigger, then advance whatever cycle is running by one slice. */
    if (g->gc_state.phase == GC_IDLE &&
        g->gc_node_count >= g->gc_node_threshold) {
        gc_start_cycle(g);
    }
    if (g->gc_state.phase != GC_IDLE) {
        gc_slice(g);
    }
}

/* =========================================================================
 * Major GC -- incremental tri-color mark-and-sweep
 * =========================================================================
 *
 *   MARK   Pop a grey object, blacken it, and grey its white children.  When
 *          the grey set empties, every reachable object is black.
 *
 *   SWEEP  Walk the major heap list: free white objects, reset black ones to
 *          white for the next cycle.
 *
 * The mutator runs between slices, so it must never hide a white object
 * behind a black one.  gc_write_barrier restores the invariant after each
 * mutation, and gc_adopt_promoted does the same for objects the minor GC
 * promotes mid-cycle.
 * ========================================================================= */

void gc_state_init(struct gc_state *s) {
    s->phase = GC_IDLE;
    node_vec_init(&s->mark_stack, 0);
    s->sweep_ptr = NULL;
}

void gc_state_free(struct gc_state *s) {
    node_vec_free(&s->mark_stack);
    s->phase = GC_IDLE;
    s->sweep_ptr = NULL;
}

/* Grey `n` and add it to the worklist.  The caller must have established
 * that `n` is a white major-heap object. */
static void gc_grey(struct gc_state *s, struct node_base *n) {
    node_set_color(n, GC_GREY);
    node_vec_push(&s->mark_stack, n);
}

/* Ensure a newly discovered reference cannot be swept.  Used where a
 * reference appears outside the mark traversal: through the write barrier,
 * or through promotion.
 *
 *   MARK   grey it, so the marker traces it and its children.
 *   SWEEP  blacken it, so it survives the pass the cursor is already making.
 *          Marking is finished by then, so there are no children to trace. */
static void gc_darken(struct gmachine *g, struct node_base *n) {
    if (!node_is_white_ptr(n) || gc_is_minor_ptr(g, n)) {
        return;
    }

    if (g->gc_state.phase == GC_MARK) {
        gc_grey(&g->gc_state, n);
    } else if (g->gc_state.phase == GC_SWEEP) {
        node_set_color(n, GC_BLACK);
    }
}

static void gc_darken_children(struct gmachine *g, struct node_base *node) {
    struct node_child_iter it;
    struct node_base **slot;

    node_children_begin(&it, node);
    while ((slot = node_children_next(&it)) != NULL) {
        gc_darken(g, *slot);
    }
}

void gc_write_barrier(struct gmachine *g, struct node_base *obj,
                      enum gc_color old_color) {
    int obj_in_minor = gc_is_minor_ptr(g, obj);

    /* Generational barrier: a major-heap object pointing into the minor heap
     * is a root for the next minor GC, which would otherwise never look at
     * it.  Minor-heap objects need no entry -- the collector scans those
     * through the graph already. */
    if (!obj_in_minor) {
        struct node_child_iter it;
        struct node_base **slot;

        node_children_begin(&it, obj);
        while ((slot = node_children_next(&it)) != NULL) {
            if (gc_is_minor_ptr(g, *slot)) {
                node_vec_push(&g->remembered_set, obj);
                break;
            }
        }
    }

    /* Incremental barrier (Dijkstra insertion barrier): a black object has
     * already been traced, so a new child would be missed.  Re-greying it
     * schedules a rescan. */
    if (g->gc_state.phase == GC_MARK && old_color == GC_BLACK &&
        !obj_in_minor) {
        gc_grey(&g->gc_state, obj);
    }
}

/* Begin a cycle by greying the roots.  Minor-heap objects are excluded: they
 * are not on the major heap's list and are never swept. */
static void gc_start_cycle(struct gmachine *g) {
    assert(g->gc_state.phase == GC_IDLE);
    g->gc_state.phase = GC_MARK;

    struct node_base **roots = stack_slots(&g->stack);
    for (size_t i = 0; i < stack_count(&g->stack); i++) {
        if (node_is_white_ptr(roots[i])) {
            gc_grey(&g->gc_state, roots[i]);
        }
    }
}

static void gc_mark_slice(struct gmachine *g) {
    struct gc_state *s = &g->gc_state;

    for (size_t work = 0;
         work < GC_MARK_SLICE && !node_vec_is_empty(&s->mark_stack); work++) {
        struct node_base *n = node_vec_pop(&s->mark_stack);
        node_set_color(n, GC_BLACK);

        struct node_child_iter it;
        struct node_base **slot;

        node_children_begin(&it, n);
        while ((slot = node_children_next(&it)) != NULL) {
            if (node_is_white_ptr(*slot)) {
                gc_grey(s, *slot);
            }
        }
    }

    if (node_vec_is_empty(&s->mark_stack)) {
        s->phase = GC_SWEEP;
        s->sweep_ptr = &g->gc_nodes;
    }
}

static void gc_sweep_slice(struct gmachine *g) {
    struct gc_state *s = &g->gc_state;

    for (size_t work = 0; work < GC_SWEEP_SLICE && *s->sweep_ptr != NULL;
         work++) {
        struct node_base *n = *s->sweep_ptr;

        if (node_color(n) == GC_BLACK) {
            node_set_color(n, GC_WHITE); /* reset for the next cycle */
            s->sweep_ptr = &n->gc_next;
        } else {
            *s->sweep_ptr = n->gc_next; /* unlink, then reclaim */
            major_release(n);
            g->gc_node_count--;
        }
    }

    if (*s->sweep_ptr == NULL) {
        s->phase = GC_IDLE;
        g->gc_node_threshold = g->gc_node_count * 2;
        if (g->gc_node_threshold < GC_MIN_THRESHOLD) {
            g->gc_node_threshold = GC_MIN_THRESHOLD;
        }
    }
}

/* Advance the major collector by one bounded slice.
 *
 * Requires an empty minor heap.  Marking greys whatever it finds, and a
 * minor-heap object would be relocated by the next promotion, leaving a
 * dangling entry in the grey set.  Both callers guarantee this: minor_gc
 * slices only after resetting the heap, and gmachine_gc flushes it first. */
static void gc_slice(struct gmachine *g) {
    assert(minor_heap_used(&g->minor_heap) == 0 &&
           "major GC slice with a non-empty minor heap");

    switch (g->gc_state.phase) {
    case GC_MARK:
        gc_mark_slice(g);
        break;
    case GC_SWEEP:
        gc_sweep_slice(g);
        break;
    case GC_IDLE:
        break;
    }
}

void gmachine_gc(struct gmachine *g) {
    /* Promote first: the slices below require an empty minor heap, and this
     * also advances any cycle already running. */
    minor_gc(g);

    if (g->gc_state.phase == GC_IDLE) {
        gc_start_cycle(g);
    }
    while (g->gc_state.phase != GC_IDLE) {
        gc_slice(g);
    }
}
