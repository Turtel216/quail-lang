#include <stdio.h>

#include "eval.h"
#include "gmachine.h"
#include "heap.h"
#include "stack.h"

/* The compiled program's entry supercombinator, emitted by the code
 * generator as "f_main". */
extern void f_main(struct gmachine *g);

int main(void) {
    struct gmachine g;

    /* Must come first: allocation needs the minor heap. */
    gmachine_init(&g);

    stack_push(&g.stack, (struct node_base *)alloc_global(&g, f_main, 0));
    unwind(&g);

    (void)printf("Result: ");
    print_node(stdout, stack_pop(&g.stack));
    (void)putchar('\n');

    gmachine_free(&g);
    return 0;
}
