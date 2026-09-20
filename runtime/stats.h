#ifndef QUAIL_RT_STATS_H_
#define QUAIL_RT_STATS_H_

#include <stdio.h>

/* =========================================================================
 * Counters.
 * =========================================================================
 *
 * What the program allocated and how often it collected, so that a claim
 * about the cost of something can be checked rather than argued.  The
 * counters always run; they are only reported when QUAIL_STATS is set in the
 * environment, which is what a benchmark reads.
 * ========================================================================= */

struct rt_stats {
    unsigned long apps;    /* application nodes */
    unsigned long globals; /* supercombinator nodes */
    unsigned long inds;    /* placeholder indirections */
    unsigned long packs;   /* data nodes, dictionaries among them */
    unsigned long minor_collections;
};

extern struct rt_stats rt_stats;

/* Write the counters to `out` if the environment asked for them. */
void rt_stats_report(FILE *out);

#endif /* QUAIL_RT_STATS_H_ */
