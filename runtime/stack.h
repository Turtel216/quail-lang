#ifndef QUAIL_RT_STACK_H_
#define QUAIL_RT_STACK_H_

#include <stddef.h>

#include "node.h"
#include "vec.h"

/* =========================================================================
 * G-machine stack.
 * =========================================================================
 *
 * Holds tagged values (unboxed integers or node pointers).  It is also the
 * collector's root set: every live object is reachable from here, so both
 * GCs scan and rewrite these slots directly.
 *
 * Generated code receives a `struct stack *` and calls the functions below;
 * the type is opaque on that side, so the layout may change freely.
 * ========================================================================= */

struct stack {
    struct node_vec items;
};

void stack_init(struct stack *s);
void stack_free(struct stack *s);

void stack_push(struct stack *s, struct node_base *n);
struct node_base *stack_pop(struct stack *s);

/* Value `o` slots down from the top; offset 0 is the topmost value. */
struct node_base *stack_peek(struct stack *s, size_t o);

/* Discard the top `n` values. */
void stack_popn(struct stack *s, size_t n);

/* Ensure `n` more values fit without reallocating mid-sequence. */
void stack_reserve(struct stack *s, size_t n);

static inline size_t stack_count(const struct stack *s) {
    return s->items.count;
}

/* Direct access to the root slots, for the collectors.  Slot `i` counts from
 * the bottom of the stack.  Invalidated by any push. */
static inline struct node_base **stack_slots(struct stack *s) {
    return s->items.data;
}

#endif /* QUAIL_RT_STACK_H_ */
