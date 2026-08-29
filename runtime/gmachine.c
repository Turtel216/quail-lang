#include "gmachine.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "panic.h"

/* Object count that triggers the first major GC cycle. */
enum { GC_INITIAL_THRESHOLD = 128 };

void gmachine_init(struct gmachine *g) {
    stack_init(&g->stack);
    minor_heap_init(&g->minor_heap);
    node_vec_init(&g->remembered_set, 0);
    gc_state_init(&g->gc_state);

    g->gc_nodes = NULL;
    g->gc_node_count = 0;
    g->gc_node_threshold = GC_INITIAL_THRESHOLD;
}

void gmachine_free(struct gmachine *g) {
    /* Flush the minor heap first.  This promotes the survivors and releases
     * the resources of everything else, leaving every remaining object on
     * the major heap list where gc_free_all can reach it. */
    minor_gc(g);

    stack_free(&g->stack);
    minor_heap_free(&g->minor_heap);
    node_vec_free(&g->remembered_set);
    gc_state_free(&g->gc_state);
    gc_free_all(g);
}

void gmachine_slide(struct gmachine *g, size_t n) {
    struct stack *s = &g->stack;
    assert(stack_count(s) > n && "slide past bottom of stack");

    struct node_base *top = stack_peek(s, 0);
    stack_popn(s, n + 1);
    stack_push(s, top);
}

void gmachine_update(struct gmachine *g, size_t o) {
    struct stack *s = &g->stack;
    assert(stack_count(s) > o + 1 && "update past bottom of stack");

    struct node_base *value = stack_pop(s);
    struct node_ind *ind = (struct node_ind *)stack_peek(s, o);

    /* The node keeps its original size.  The minor heap is walked linearly
     * using the size field, so shrinking it here (an application is four
     * words, an indirection three) would desynchronise that walk.  The color
     * is preserved because the collector's invariants are stated in terms of
     * it; the write barrier below needs the value it had. */
    enum gc_color color = node_color(&ind->base);
    ind->base.header = hdr_make(NODE_IND, color, hdr_size_words(ind->base.header));
    ind->next = value;

    gc_write_barrier(g, &ind->base, color);
}

void gmachine_alloc(struct gmachine *g, size_t o) {
    while (o-- > 0) {
        stack_push(&g->stack, (struct node_base *)alloc_ind(g, NULL));
    }
}

void gmachine_pack(struct gmachine *g, size_t n, int8_t t) {
    struct stack *s = &g->stack;
    assert(stack_count(s) >= n && "pack past bottom of stack");

    /* Allocate before reading the stack: this may run a minor GC, which
     * relocates the field values and rewrites the slots holding them. */
    struct node_data *node =
        (struct node_data *)minor_alloc(g, sizeof *node);
    node->base.header = hdr_make_data(GC_WHITE, t, WORDS_NODE_DATA);
    node->array = NULL;

    if (n > SIZE_MAX / sizeof *node->array - 1) {
        rt_fatal("pack arity overflow");
    }

    /* NULL-terminated so the collector can walk the fields without needing
     * the arity.  Owned by the node; released with it. */
    struct node_base **fields = malloc((n + 1) * sizeof *fields);
    if (fields == NULL) {
        rt_oom("data constructor fields");
    }
    memcpy(fields, &stack_slots(s)[stack_count(s) - n], n * sizeof *fields);
    fields[n] = NULL;
    node->array = fields;

    stack_popn(s, n);
    stack_push(s, &node->base);
}

void gmachine_split(struct gmachine *g, size_t n) {
    struct stack *s = &g->stack;

    struct node_data *node = (struct node_data *)stack_pop(s);
    assert(node_tag_of(&node->base) == NODE_DATA && "split of a non-data node");

    /* Reserve up front: the fields live in an array the pushes do not touch,
     * but growing the stack mid-loop would be a needless reallocation. */
    stack_reserve(s, n);
    for (size_t i = 0; i < n; i++) {
        stack_push(s, node->array[i]);
    }
}
