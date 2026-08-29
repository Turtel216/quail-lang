#ifndef QUAIL_RT_NODE_H_
#define QUAIL_RT_NODE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* =========================================================================
 * Graph nodes: value tagging, object header, layouts, child traversal.
 * =========================================================================
 *
 * This header is the single source of truth for how a G-machine value is
 * represented.  Everything else in the runtime -- the allocator, both
 * collectors, the evaluator -- goes through the accessors defined here
 * rather than open-coding shifts and masks.
 * ========================================================================= */

struct gmachine;
struct node_base;

/* -- Value tagging (OCaml-style unboxed integers) --------------------------
 *
 * Every G-machine stack slot holds a tagged value typed as
 * `struct node_base *`.  The least significant bit discriminates:
 *
 *   LSB == 1  ->  unboxed integer  (value in the upper 63 bits, shifted << 1)
 *   LSB == 0  ->  pointer to a heap-allocated node (naturally word-aligned)
 *
 * A tagged integer is never dereferenced, so forming one is safe even though
 * it is not a valid address.  The generated code creates and consumes these
 * inline (see CodeGenerator::createNum / unwrapNum); the definitions here
 * must stay bit-compatible with it.
 */

static inline int node_is_int(const struct node_base *v) {
    return (int)((uintptr_t)v & 1u);
}

static inline int node_is_ptr(const struct node_base *v) {
    return !node_is_int(v);
}

/* Encode a C integer as a tagged value: shift left by one, set the LSB. */
static inline struct node_base *node_from_int(int64_t n) {
    return (struct node_base *)(uintptr_t)(((uintptr_t)n << 1) | 1u);
}

/* Decode a tagged value back to a C integer.  The arithmetic right shift
 * sign-extends, so negative values round-trip. */
static inline int64_t node_to_int(const struct node_base *v) {
    return (int64_t)((intptr_t)v >> 1);
}

/* -- Node tags -------------------------------------------------------------
 * Stored in bits [1:0] of the header.  Two bits -> four node kinds.
 * The explicit values are part of the bit encoding; do not renumber. */
enum node_tag { NODE_APP = 0, NODE_GLOBAL = 1, NODE_IND = 2, NODE_DATA = 3 };

/* -- GC colors (tri-color marking) ----------------------------------------
 * Stored in bits [3:2] of the header. */
enum gc_color { GC_WHITE = 0, GC_GREY = 1, GC_BLACK = 2 };

/* -- Unified object header -------------------------------------------------
 *
 * All heap-allocated nodes share one 64-bit header word:
 *
 *   Bits [1:0]   Tag       node kind (APP, GLOBAL, IND, DATA)
 *   Bits [3:2]   Color     GC tri-color (WHITE, GREY, BLACK)
 *   Bits [11:4]  Data tag  data constructor tag (NODE_DATA only, 8 bits)
 *   Bits [15:12] Size      object size in 8-byte words (max 15 -> 120 bytes)
 *   Bits [63:16] Fwd       forwarding address for the copying minor GC (>> 3)
 *
 * The size field lives below the forwarding field on purpose: once a minor
 * GC installs a forwarding pointer the low 16 bits are still intact, so the
 * linear sweep over the minor heap can keep reading the object size.
 */

enum {
    HDR_TAG_SHIFT = 0,  HDR_TAG_BITS = 2,
    HDR_COLOR_SHIFT = 2, HDR_COLOR_BITS = 2,
    HDR_DATA_TAG_SHIFT = 4, HDR_DATA_TAG_BITS = 8,
    HDR_SIZE_SHIFT = 12, HDR_SIZE_BITS = 4,
    HDR_FWD_SHIFT = 16, HDR_FWD_ALIGN_BITS = 3
};

#define HDR_MASK(bits) ((uint64_t)((1ULL << (bits)) - 1ULL))

/* Largest object size the 4-bit size field can express, in 8-byte words. */
#define NODE_MAX_SIZE_WORDS ((size_t)HDR_MASK(HDR_SIZE_BITS))

/* -- Header field reads --------------------------------------------------- */

static inline enum node_tag hdr_tag(uint64_t h) {
    return (enum node_tag)((h >> HDR_TAG_SHIFT) & HDR_MASK(HDR_TAG_BITS));
}

static inline enum gc_color hdr_color(uint64_t h) {
    return (enum gc_color)((h >> HDR_COLOR_SHIFT) & HDR_MASK(HDR_COLOR_BITS));
}

/* The data constructor tag is stored as raw bits; the round trip through
 * uint8_t reproduces the original signed value on any two's-complement
 * target, which the project already requires. */
static inline int8_t hdr_data_tag(uint64_t h) {
    return (int8_t)(uint8_t)((h >> HDR_DATA_TAG_SHIFT) &
                             HDR_MASK(HDR_DATA_TAG_BITS));
}

static inline size_t hdr_size_words(uint64_t h) {
    return (size_t)((h >> HDR_SIZE_SHIFT) & HDR_MASK(HDR_SIZE_BITS));
}

/* True once a minor GC has moved this object and left a forwarding address. */
static inline int hdr_is_fwd(uint64_t h) { return (h >> HDR_FWD_SHIFT) != 0; }

static inline struct node_base *hdr_fwd(uint64_t h) {
    return (struct node_base *)(uintptr_t)((h >> HDR_FWD_SHIFT)
                                           << HDR_FWD_ALIGN_BITS);
}

/* -- Header construction and mutation ------------------------------------- */

static inline uint64_t hdr_make(enum node_tag tag, enum gc_color color,
                                size_t size_words) {
    return ((uint64_t)tag << HDR_TAG_SHIFT) |
           ((uint64_t)color << HDR_COLOR_SHIFT) |
           ((uint64_t)size_words << HDR_SIZE_SHIFT);
}

static inline uint64_t hdr_make_data(enum gc_color color, int8_t data_tag,
                                     size_t size_words) {
    return hdr_make(NODE_DATA, color, size_words) |
           (((uint64_t)(uint8_t)data_tag) << HDR_DATA_TAG_SHIFT);
}

static inline uint64_t hdr_with_color(uint64_t h, enum gc_color color) {
    return (h & ~(HDR_MASK(HDR_COLOR_BITS) << HDR_COLOR_SHIFT)) |
           ((uint64_t)color << HDR_COLOR_SHIFT);
}

/* Install a forwarding address, preserving tag/color/data-tag/size. */
static inline uint64_t hdr_with_fwd(uint64_t h, const struct node_base *to) {
    return (h & HDR_MASK(HDR_FWD_SHIFT)) |
           (((uint64_t)(uintptr_t)to >> HDR_FWD_ALIGN_BITS) << HDR_FWD_SHIFT);
}

/* -- Node layouts ----------------------------------------------------------
 *
 * `node_base` is the common prefix of every heap node.  The generated code
 * relies on `header` being at offset 0 (it loads the data constructor tag
 * straight out of it), so that field must stay first.
 *
 * `gc_next` threads every major-heap object onto an intrusive list that the
 * sweep phase walks.
 */

struct node_base {
    uint64_t header;           /* tag | color | data tag | size | fwd */
    struct node_base *gc_next; /* intrusive major-heap sweep list */
};

/* Application: `left` applied to `right`. */
struct node_app {
    struct node_base base;
    struct node_base *left;
    struct node_base *right;
};

/* Supercombinator: a code pointer plus the number of arguments it consumes.
 * Members are ordered largest-first to avoid interior padding. */
struct node_global {
    struct node_base base;
    void (*function)(struct gmachine *);
    int32_t arity;
};

/* Indirection: the result of updating a redex in place. */
struct node_ind {
    struct node_base base;
    struct node_base *next;
};

/* Saturated data constructor.  The constructor tag lives in the header's
 * bits [11:4]; `array` is a separately malloc'd, NULL-terminated vector of
 * field values owned by this node. */
struct node_data {
    struct node_base base;
    struct node_base **array;
};

/* -- Sizes ---------------------------------------------------------------- */

#define NODE_WORDS(type) ((size_t)(sizeof(type) / sizeof(uint64_t)))

#define WORDS_NODE_APP    NODE_WORDS(struct node_app)
#define WORDS_NODE_GLOBAL NODE_WORDS(struct node_global)
#define WORDS_NODE_IND    NODE_WORDS(struct node_ind)
#define WORDS_NODE_DATA   NODE_WORDS(struct node_data)

/* -- Node-level convenience accessors ------------------------------------- */

static inline enum node_tag node_tag_of(const struct node_base *n) {
    return hdr_tag(n->header);
}

static inline enum gc_color node_color(const struct node_base *n) {
    return hdr_color(n->header);
}

static inline void node_set_color(struct node_base *n, enum gc_color color) {
    n->header = hdr_with_color(n->header, color);
}

static inline size_t node_size_bytes(const struct node_base *n) {
    return hdr_size_words(n->header) * sizeof(uint64_t);
}

/* True for a value the collector can actually follow.  Rules out unboxed
 * integers and NULL, which appears as the target of the placeholder
 * indirections that the Alloc instruction pushes. */
static inline int node_is_heap_ptr(const struct node_base *v) {
    return v != NULL && node_is_ptr(v);
}

/* True for a traceable heap pointer the GC has not yet reached this cycle. */
static inline int node_is_white_ptr(const struct node_base *v) {
    return node_is_heap_ptr(v) && node_color(v) == GC_WHITE;
}

/* Release memory owned by, but not part of, the node itself.  Only
 * NODE_DATA has any: its field array. */
static inline void node_free_resources(struct node_base *n) {
    if (node_tag_of(n) == NODE_DATA) {
        free(((struct node_data *)n)->array);
        ((struct node_data *)n)->array = NULL;
    }
}

/* -- Child traversal -------------------------------------------------------
 *
 * The minor GC (scavenge), the major GC (mark), and the write barrier
 * (darken) all need to visit a node's GC-managed children.  They differ only
 * in what they do with each child, so the layout knowledge lives here once.
 *
 * Iteration yields the *address* of each child slot, which lets the copying
 * collector update it in place:
 *
 *     struct node_child_iter it;
 *     struct node_base **slot;
 *     node_children_begin(&it, node);
 *     while ((slot = node_children_next(&it)) != NULL) {
 *         *slot = evacuate(g, *slot);
 *     }
 *
 * Fixed-arity nodes keep their slots inside the node; NODE_DATA keeps them
 * in an external NULL-terminated array.  Slot addresses are stored
 * individually rather than walked with pointer arithmetic, since arithmetic
 * across separate struct members is not defined behaviour.
 */

enum { NODE_MAX_FIXED_CHILDREN = 2 }; /* NODE_APP has the most */

struct node_child_iter {
    struct node_base **fixed[NODE_MAX_FIXED_CHILDREN];
    size_t fixed_count;
    size_t next;
    struct node_base **array; /* NULL-terminated; NULL when unused */
};

static inline void node_children_begin(struct node_child_iter *it,
                                       struct node_base *n) {
    it->fixed_count = 0;
    it->next = 0;
    it->array = NULL;

    switch (node_tag_of(n)) {
    case NODE_APP: {
        struct node_app *app = (struct node_app *)n;
        it->fixed[0] = &app->left;
        it->fixed[1] = &app->right;
        it->fixed_count = 2;
        break;
    }
    case NODE_IND: {
        struct node_ind *ind = (struct node_ind *)n;
        it->fixed[0] = &ind->next;
        it->fixed_count = 1;
        break;
    }
    case NODE_DATA:
        it->array = ((struct node_data *)n)->array;
        break;
    case NODE_GLOBAL:
        /* A code pointer and an arity: nothing for the GC to follow. */
        break;
    }
}

/* Returns the next child slot, or NULL when the node is exhausted. */
static inline struct node_base **
node_children_next(struct node_child_iter *it) {
    if (it->array != NULL) {
        return (*it->array != NULL) ? it->array++ : NULL;
    }
    return (it->next < it->fixed_count) ? it->fixed[it->next++] : NULL;
}

#endif /* QUAIL_RT_NODE_H_ */
