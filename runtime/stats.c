#include "stats.h"

#include <stdlib.h>

struct rt_stats rt_stats = {0, 0, 0, 0, 0};

void rt_stats_report(FILE *out) {
    if (getenv("QUAIL_STATS") == NULL) {
        return;
    }

    (void)fprintf(out, "apps %lu\n", rt_stats.apps);
    (void)fprintf(out, "globals %lu\n", rt_stats.globals);
    (void)fprintf(out, "inds %lu\n", rt_stats.inds);
    (void)fprintf(out, "packs %lu\n", rt_stats.packs);
    (void)fprintf(out, "minor %lu\n", rt_stats.minor_collections);
}
