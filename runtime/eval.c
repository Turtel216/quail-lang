#include "eval.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>

#include "stack.h"

/* Enter a supercombinator.
 *
 * The stack holds the spine of applications above the global node.  Rewrite
 * each spine slot in place with the argument that application supplies, so
 * the callee sees its parameters directly, then run its code.
 *
 *   before:  ... (f a b)  (f a)  f          <- top
 *   after:   ... (f a b)   b      a         <- top
 */
static void unwind_global(struct gmachine *g, struct node_global *global) {
    struct stack *s = &g->stack;

    assert(global->arity >= 0 && "negative arity");
    size_t arity = (size_t)global->arity;
    assert(stack_count(s) > arity && "unwinding an under-applied global");

    struct node_base **slots = stack_slots(s);
    size_t top = stack_count(s);

    for (size_t i = 1; i <= arity; i++) {
        struct node_app *app = (struct node_app *)slots[top - i - 1];
        assert(node_tag_of(&app->base) == NODE_APP && "malformed spine");
        slots[top - i] = app->right;
    }

    /* May allocate, collect, and reshape the stack; nothing above is reused. */
    global->function(g);
}

void unwind(struct gmachine *g) {
    struct stack *s = &g->stack;

    for (;;) {
        struct node_base *top = stack_peek(s, 0);

        /* An unboxed integer is already in weak head normal form. */
        if (node_is_int(top)) {
            return;
        }

        switch (node_tag_of(top)) {
        case NODE_APP:
            /* Walk down the spine toward the function. */
            stack_push(s, ((struct node_app *)top)->left);
            break;

        case NODE_GLOBAL:
            unwind_global(g, (struct node_global *)top);
            break;

        case NODE_IND:
            /* Skip the indirection left behind by a previous update. */
            (void)stack_pop(s);
            stack_push(s, ((struct node_ind *)top)->next);
            break;

        case NODE_DATA:
            /* A saturated constructor is already in weak head normal form. */
            return;
        }
    }
}

void print_node(FILE *out, struct node_base *n) {
    if (node_is_int(n)) {
        (void)fprintf(out, "%" PRId64, node_to_int(n));
        return;
    }

    switch (node_tag_of(n)) {
    case NODE_APP: {
        struct node_app *app = (struct node_app *)n;
        print_node(out, app->left);
        (void)fputc(' ', out);
        print_node(out, app->right);
        break;
    }
    case NODE_GLOBAL:
        (void)fprintf(out, "(Global: %p)",
                      (void *)(uintptr_t)((struct node_global *)n)->function);
        break;
    case NODE_IND:
        print_node(out, ((struct node_ind *)n)->next);
        break;
    case NODE_DATA:
        (void)fprintf(out, "(Packed)");
        break;
    }
}
