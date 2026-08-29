#include "heap.h"

#include <assert.h>
#include <stdlib.h>

#include "gc.h"
#include "gmachine.h"
#include "panic.h"

/* Node layouts are constrained by the header's 4-bit size field and by the
 * generated code, which assumes the header sits at offset 0. */
_Static_assert(sizeof(struct node_base) == 16,
               "node_base must be 16 bytes (header + gc_next)");
_Static_assert(WORDS_NODE_APP <= NODE_MAX_SIZE_WORDS, "node_app too large");
_Static_assert(WORDS_NODE_GLOBAL <= NODE_MAX_SIZE_WORDS,
               "node_global too large");
_Static_assert(WORDS_NODE_IND <= NODE_MAX_SIZE_WORDS, "node_ind too large");
_Static_assert(WORDS_NODE_DATA <= NODE_MAX_SIZE_WORDS, "node_data too large");

void minor_heap_init(struct minor_heap *h) {
    h->start = malloc((size_t)MINOR_HEAP_SIZE);
    if (h->start == NULL) {
        rt_oom("minor heap");
    }
    h->limit = h->start + MINOR_HEAP_SIZE;
    h->top = h->limit; /* empty; grows downward */
}

void minor_heap_free(struct minor_heap *h) {
    free(h->start);
    h->start = NULL;
    h->top = NULL;
    h->limit = NULL;
}

struct node_base *minor_alloc(struct gmachine *g, size_t size_bytes) {
    assert(size_bytes >= sizeof(struct node_base));
    assert(size_bytes % sizeof(uint64_t) == 0);

    struct minor_heap *h = &g->minor_heap;
    uint8_t *top = h->top - size_bytes;

    if (top < h->start) {
        minor_gc(g);
        top = h->top - size_bytes;
        if (top < h->start) {
            rt_fatal("minor heap overflow: object larger than the whole heap");
        }
    }

    h->top = top;

    struct node_base *node = (struct node_base *)top;
    node->header = 0;
    node->gc_next = NULL;
    return node;
}

/* Allocate `size_bytes`, keeping `roots[0..count)` alive across a minor GC.
 *
 * Parking the values on the G-machine stack makes them roots, so a collection
 * inside minor_alloc relocates them along with everything else; unparking
 * writes the post-GC addresses back into `roots`.  Unboxed integers and NULL
 * ride along harmlessly, so callers need not filter them out. */
static struct node_base *alloc_protecting(struct gmachine *g, size_t size_bytes,
                                          struct node_base **roots,
                                          size_t count) {
    for (size_t i = 0; i < count; i++) {
        stack_push(&g->stack, roots[i]);
    }

    struct node_base *node = minor_alloc(g, size_bytes);

    for (size_t i = count; i-- > 0;) {
        roots[i] = stack_pop(&g->stack);
    }

    return node;
}

struct node_app *alloc_app(struct gmachine *g, struct node_base *l,
                           struct node_base *r) {
    struct node_base *roots[2] = {l, r};

    struct node_app *node =
        (struct node_app *)alloc_protecting(g, sizeof *node, roots, 2);

    node->base.header = hdr_make(NODE_APP, GC_WHITE, WORDS_NODE_APP);
    node->left = roots[0];
    node->right = roots[1];
    return node;
}

struct node_global *alloc_global(struct gmachine *g,
                                 void (*f)(struct gmachine *), int32_t a) {
    /* A code pointer and an arity: neither is collector-managed. */
    struct node_global *node =
        (struct node_global *)alloc_protecting(g, sizeof *node, NULL, 0);

    node->base.header = hdr_make(NODE_GLOBAL, GC_WHITE, WORDS_NODE_GLOBAL);
    node->function = f;
    node->arity = a;
    return node;
}

struct node_ind *alloc_ind(struct gmachine *g, struct node_base *n) {
    struct node_base *roots[1] = {n};

    struct node_ind *node =
        (struct node_ind *)alloc_protecting(g, sizeof *node, roots, 1);

    node->base.header = hdr_make(NODE_IND, GC_WHITE, WORDS_NODE_IND);
    node->next = roots[0];
    return node;
}
