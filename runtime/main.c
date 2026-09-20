#include <stdio.h>

#include "eval.h"
#include "gmachine.h"
#include "heap.h"
#include "stack.h"
#include "stats.h"

/* The compiled program's entry supercombinator, emitted by the code
 * generator as "f_main". */
extern void f_main(struct gmachine *g);

/* Fills in and registers the program's shared values.  Emitted by the code
 * generator whether or not the program has any. */
extern void quail_init_cafs(struct gmachine *g);

int main(void) {
    struct gmachine g;

    /* Must come first: allocation needs the minor heap. */
    gmachine_init(&g);

    /* Before anything is pushed, so that every reference to a shared value
     * finds the one node standing for it. */
    quail_init_cafs(&g);

    stack_push(&g.stack, (struct node_base *)alloc_global(&g, f_main, 0));
    unwind(&g);

    (void)printf("Result: ");
    print_node(stdout, stack_pop(&g.stack));
    (void)putchar('\n');

    rt_stats_report(stderr);

    gmachine_free(&g);
    return 0;
}
