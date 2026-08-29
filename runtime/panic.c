#include "panic.h"

#include <stdio.h>
#include <stdlib.h>

_Noreturn void rt_fatal(const char *msg) {
    (void)fprintf(stderr, "quail runtime: fatal: %s\n", msg);
    abort();
}

_Noreturn void rt_oom(const char *what) {
    (void)fprintf(stderr, "quail runtime: out of memory allocating %s\n", what);
    abort();
}
