#ifndef QUAIL_RT_HEAP_H_
#define QUAIL_RT_HEAP_H_

#include <stddef.h>
#include <stdint.h>

#include "node.h"

struct gmachine;

/* =========================================================================
 * Minor heap -- bump allocator.
 * =========================================================================
 *
 * A contiguous region allocated once at startup.  Allocation runs downward,
 * from high addresses to low (as in OCaml), so the fast path is a single
 * subtract-and-compare:
 *
 *   start                       top                              limit
 *     |       free space         |        allocated objects        |
 *     v                          v                                 v
 *     [__________________________|+++++++++++++++++++++++++++++++++++]
 *
 * When the region fills, a minor GC promotes the survivors to the major heap
 * and resets `top` back to `limit`.
 * ========================================================================= */

enum { MINOR_HEAP_SIZE = 2 * 1024 * 1024 }; /* 2 MiB */

struct minor_heap {
    uint8_t *start; /* low address: start of the region */
    uint8_t *top;   /* allocation pointer; decrements toward `start` */
    uint8_t *limit; /* high address: one past the end of the region */
};

void minor_heap_init(struct minor_heap *h);
void minor_heap_free(struct minor_heap *h);

/* Drop every object at once.  Only valid after the survivors have been
 * promoted, since nothing else records their addresses. */
static inline void minor_heap_reset(struct minor_heap *h) {
    h->top = h->limit;
}

static inline size_t minor_heap_used(const struct minor_heap *h) {
    return (size_t)(h->limit - h->top);
}

/* Whether `p` addresses an object in this heap.  `p` may be a tagged
 * integer, in which case the comparison simply fails. */
static inline int minor_heap_contains(const struct minor_heap *h,
                                      const void *p) {
    const uint8_t *b = (const uint8_t *)p;
    return b >= h->start && b < h->limit;
}

/* -- Linear walk -----------------------------------------------------------
 *
 * Objects are laid out contiguously, so the heap can be walked in allocation
 * order without any side table: each header records the object's size.  The
 * minor GC uses this to reclaim resources owned by objects that did not
 * survive.  Sizes stay readable after forwarding because the forwarding
 * address occupies bits above the size field.
 */

static inline struct node_base *minor_heap_first(const struct minor_heap *h) {
    return (h->top < h->limit) ? (struct node_base *)h->top : NULL;
}

static inline struct node_base *minor_heap_next(const struct minor_heap *h,
                                                struct node_base *n) {
    uint8_t *next = (uint8_t *)n + node_size_bytes(n);
    return (next < h->limit) ? (struct node_base *)next : NULL;
}

/* -- Allocation ------------------------------------------------------------
 *
 * All allocation goes through the minor heap.  Any of these may trigger a
 * minor GC, which relocates live objects, so a caller holding a node pointer
 * across an allocation must keep it on the G-machine stack where the
 * collector can find and update it.  The alloc_* wrappers do this for the
 * arguments they are handed.
 *
 * Ownership: allocated nodes belong to the collector.  Callers never free
 * them.
 */

/* Reserve `size_bytes` (a whole number of 8-byte words) and return a node
 * with a zeroed header.  The caller must install a real header immediately,
 * with no intervening allocation: the heap walk above cannot step over an
 * object whose size field is still zero. */
struct node_base *minor_alloc(struct gmachine *g, size_t size_bytes);

/* -- Node constructors (also called directly by generated code) ----------- */

struct node_app *alloc_app(struct gmachine *g, struct node_base *l,
                           struct node_base *r);
struct node_global *alloc_global(struct gmachine *g,
                                 void (*f)(struct gmachine *), int32_t a);
struct node_ind *alloc_ind(struct gmachine *g, struct node_base *n);

#endif /* QUAIL_RT_HEAP_H_ */
