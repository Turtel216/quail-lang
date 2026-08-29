#ifndef QUAIL_RT_EVAL_H_
#define QUAIL_RT_EVAL_H_

#include <stdio.h>

#include "gmachine.h"
#include "node.h"

/* =========================================================================
 * Graph reduction.
 * ========================================================================= */

/* Reduce the value on top of the stack to weak head normal form.
 *
 * Called by generated code (the Unwind and Eval instructions). */
void unwind(struct gmachine *g);

/* Print a reduced value in source-like form to `out`. */
void print_node(FILE *out, struct node_base *n);

#endif /* QUAIL_RT_EVAL_H_ */
