#include "vec.h"

#include <stdint.h>
#include <stdlib.h>

#include "panic.h"

/* Capacity used by the first growth of a vector initialised empty. */
enum { NODE_VEC_MIN_CAPACITY = 16 };

/* Grow the backing array to hold at least `needed` elements. */
static void node_vec_grow_to(struct node_vec *v, size_t needed) {
    size_t capacity = (v->capacity == 0) ? NODE_VEC_MIN_CAPACITY : v->capacity;

    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            rt_fatal("node_vec capacity overflow");
        }
        capacity *= 2;
    }

    if (capacity > SIZE_MAX / sizeof *v->data) {
        rt_fatal("node_vec capacity overflow");
    }

    struct node_base **tmp = realloc(v->data, capacity * sizeof *tmp);
    if (tmp == NULL) {
        rt_oom("node_vec");
    }

    v->data = tmp;
    v->capacity = capacity;
}

void node_vec_init(struct node_vec *v, size_t capacity) {
    v->data = NULL;
    v->count = 0;
    v->capacity = 0;

    if (capacity > 0) {
        node_vec_grow_to(v, capacity);
    }
}

void node_vec_free(struct node_vec *v) {
    free(v->data);
    v->data = NULL;
    v->count = 0;
    v->capacity = 0;
}

void node_vec_push(struct node_vec *v, struct node_base *n) {
    if (v->count == v->capacity) {
        node_vec_grow_to(v, v->count + 1);
    }
    v->data[v->count++] = n;
}

void node_vec_reserve(struct node_vec *v, size_t extra) {
    if (extra > SIZE_MAX - v->count) {
        rt_fatal("node_vec capacity overflow");
    }
    if (v->count + extra > v->capacity) {
        node_vec_grow_to(v, v->count + extra);
    }
}
