#ifndef QUAIL_RT_VEC_H_
#define QUAIL_RT_VEC_H_

#include <stddef.h>

#include "node.h"

/* =========================================================================
 * node_vec -- growable array of tagged values.
 * =========================================================================
 *
 * The runtime needs the same push/grow array in four places: the G-machine
 * stack, the remembered set, the major GC's mark stack, and the minor GC's
 * Cheney scan queue.  They all live here.
 *
 * Ownership: the vector owns `data`.  node_vec_free releases it; the
 * elements themselves are owned by the heap, never by the vector.
 * ========================================================================= */

struct node_vec {
    struct node_base **data;
    size_t count;
    size_t capacity;
};

/* Initialise an empty vector with room for `capacity` elements.  A capacity
 * of 0 defers the allocation until the first push. */
void node_vec_init(struct node_vec *v, size_t capacity);

/* Release the backing array.  Safe to call twice. */
void node_vec_free(struct node_vec *v);

/* Append `n`, growing the backing array if needed.  Aborts on OOM. */
void node_vec_push(struct node_vec *v, struct node_base *n);

/* Ensure room for at least `extra` more elements without reallocating. */
void node_vec_reserve(struct node_vec *v, size_t extra);

static inline int node_vec_is_empty(const struct node_vec *v) {
    return v->count == 0;
}

/* Remove and return the last element.  The vector must be non-empty. */
static inline struct node_base *node_vec_pop(struct node_vec *v) {
    return v->data[--v->count];
}

/* Drop all elements.  Keeps the backing array for reuse. */
static inline void node_vec_clear(struct node_vec *v) { v->count = 0; }

#endif /* QUAIL_RT_VEC_H_ */
