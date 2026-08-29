#include "stack.h"

#include <assert.h>

/* Deep enough for the innermost unwind of a small combinator without
 * reallocating; grows geometrically from there. */
enum { STACK_INITIAL_CAPACITY = 16 };

void stack_init(struct stack *s) {
    node_vec_init(&s->items, STACK_INITIAL_CAPACITY);
}

void stack_free(struct stack *s) { node_vec_free(&s->items); }

void stack_push(struct stack *s, struct node_base *n) {
    node_vec_push(&s->items, n);
}

struct node_base *stack_pop(struct stack *s) {
    assert(!node_vec_is_empty(&s->items) && "pop from empty stack");
    return node_vec_pop(&s->items);
}

struct node_base *stack_peek(struct stack *s, size_t o) {
    assert(stack_count(s) > o && "peek past bottom of stack");
    return s->items.data[stack_count(s) - o - 1];
}

void stack_reserve(struct stack *s, size_t n) {
    node_vec_reserve(&s->items, n);
}

void stack_popn(struct stack *s, size_t n) {
    assert(stack_count(s) >= n && "pop past bottom of stack");
    s->items.count -= n;
}
