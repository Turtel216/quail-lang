#ifndef RUNTIME_H_
#define RUNTIME_H_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* Value Tagging
 *
 * Every slot on the G-machine stack holds a "tagged value" stored as a
 * struct node_base*.  The least significant bit (LSB) discriminates:
 *
 *   LSB == 1  ->  unboxed integer  (value in upper 63 bits, shifted left by 1)
 *   LSB == 0  ->  pointer to a heap-allocated node (naturally word-aligned)
 */

#define IS_INT(v) ((uintptr_t)(v) & 1)
#define IS_PTR(v) (!IS_INT(v))

/* Encode a C integer into a tagged value.  Shift left by 1 and set LSB. */
#define MAKE_INT(n) ((struct node_base *)(((intptr_t)(n) << 1) | 1))

/* Decode a tagged value back to a C integer.  Arithmetic right-shift
 * preserves the sign bit. */
#define VAL_INT(v) ((int64_t)((intptr_t)(v) >> 1))

/* Assert that a tagged value is a real pointer before dereferencing. */
#define AS_PTR(v)                                                              \
  (assert(IS_PTR(v) && "expected heap pointer, got tagged int"), (v))

/* Node Tags
 * Encoded in bits [1:0] of the header word.  2 bits → 4 node types.
 * Explicit values ensure they match the bit encoding.
 */
enum node_tag { NODE_APP = 0, NODE_GLOBAL = 1, NODE_IND = 2, NODE_DATA = 3 };

/* GC Colors (tri-color marking)
 * Encoded in bits [3:2] of the header word.
 */
enum gc_color { GC_WHITE = 0, GC_GREY = 1, GC_BLACK = 2 };

/* Unified Object Header
 *
 * All heap-allocated graph nodes share a common 64-bit header word,
 * packed as follows:
 *
 *  63                    16  15     12  11          4  3    2  1    0
 * ┌────────────────────────┬─────────┬──────────────┬──────┬───────┐
 * │  Forwarding (48 bits)  │Size (4b)│ Data Tag (8b)│Color │  Tag  │
 * │  (ptr >> 3)            │(words)  │ (NODE_DATA)  │(2bit)│ (2bit)│
 * └────────────────────────┴─────────┴──────────────┴──────┴───────┘
 *
 * - Tag:         Node type (NODE_APP, NODE_GLOBAL, NODE_IND, NODE_DATA)
 * - Color:       GC tri-color (WHITE, GREY, BLACK)
 * - Data Tag:    Data constructor tag, meaningful only for NODE_DATA (8 bits)
 * - Size:        Object size in 8-byte words (max 15 words = 120 bytes)
 * - Forwarding:  Forwarding pointer for copying GC (Phase 4).
 *                Non-zero means "object has been evacuated to this address."
 */

/* Header field extraction */
#define HDR_TAG(h) ((enum node_tag)((h) & 0x3))
#define HDR_COLOR(h) ((enum gc_color)(((h) >> 2) & 0x3))
#define HDR_DATA_TAG(h) ((int8_t)(((h) >> 4) & 0xFF))
#define HDR_SIZE(h) ((size_t)(((h) >> 12) & 0xF))
#define HDR_FWD(h) ((struct node_base *)(uintptr_t)(((h) >> 16) << 3))

/* Header construction */
#define MAKE_HEADER(tag, color, size_words)                                    \
  ((uint64_t)(tag) | ((uint64_t)(color) << 2) | ((uint64_t)(size_words) << 12))

/* Header for NODE_DATA with an embedded data constructor tag. */
#define MAKE_HEADER_DATA(color, dtag, size_words)                              \
  ((uint64_t)NODE_DATA | ((uint64_t)(color) << 2) |                            \
   ((uint64_t)((dtag) & 0xFF) << 4) | ((uint64_t)(size_words) << 12))

/* Header field mutation (returns new header value) */
#define HDR_SET_COLOR(h, c) (((h) & ~(0x3ULL << 2)) | ((uint64_t)(c) << 2))

#define HDR_SET_FWD(h, ptr)                                                    \
  (((h) & 0xFFFFULL) | (((uint64_t)(uintptr_t)(ptr) >> 3) << 16))

/* Forwarding pointer check */
#define HDR_IS_FWD(h) (((h) >> 16) != 0)

/* Forward declarations */
struct stack;
struct gmachine;

/* Node structures
 *
 * node_base is the common header for all heap-allocated graph nodes.
 * It contains the packed 64-bit header word and an intrusive linked-list
 * pointer for the GC's sweep list.
 */

struct node_base {
  uint64_t header;           /* packed: tag | color | data_tag | size | fwd */
  struct node_base *gc_next; /* intrusive list for major heap sweep */
};

struct node_app {
  struct node_base base;
  struct node_base *left;
  struct node_base *right;
};

struct node_global {
  struct node_base base;
  int32_t arity;
  void (*function)(struct gmachine *);
};

struct node_ind {
  struct node_base base;
  struct node_base *next;
};

/* Data constructor tag is stored in the header's spare bits [11:4],
 * accessed via HDR_DATA_TAG(base.header). */
struct node_data {
  struct node_base base;
  struct node_base **array;
};

/* Node size constants (in 8-byte words) */
#define WORDS_NODE_APP ((uint64_t)(sizeof(struct node_app) / sizeof(uint64_t)))
#define WORDS_NODE_GLOBAL                                                      \
  ((uint64_t)(sizeof(struct node_global) / sizeof(uint64_t)))
#define WORDS_NODE_IND ((uint64_t)(sizeof(struct node_ind) / sizeof(uint64_t)))
#define WORDS_NODE_DATA                                                        \
  ((uint64_t)(sizeof(struct node_data) / sizeof(uint64_t)))

/* Allocation */
struct node_base *alloc_node();
struct node_app *alloc_app(struct node_base *l, struct node_base *r);
struct node_global *alloc_global(void (*f)(struct gmachine *), int32_t a);
struct node_ind *alloc_ind(struct node_base *n);

/* Stack */
struct stack {
  size_t size;
  size_t count;
  struct node_base **data;
};

void stack_init(struct stack *s);
void stack_free(struct stack *s);
void stack_push(struct stack *s, struct node_base *n);
struct node_base *stack_pop(struct stack *s);
struct node_base *stack_peek(struct stack *s, size_t o);
void stack_popn(struct stack *s, size_t n);
void stack_slide(struct stack *s, size_t n);
void stack_update(struct stack *s, size_t o);
void stack_alloc(struct stack *s, size_t o);
void stack_pack(struct stack *s, size_t n, int8_t t);
void stack_split(struct stack *s, size_t n);

/* G-Machine */
struct gmachine {
  struct stack stack;
  struct node_base *gc_nodes;
  int64_t gc_node_count;
  int64_t gc_node_threshold;
};

void gmachine_init(struct gmachine *g);
void gmachine_free(struct gmachine *g);
void gmachine_slide(struct gmachine *g, size_t n);
void gmachine_update(struct gmachine *g, size_t o);
void gmachine_alloc(struct gmachine *g, size_t o);
void gmachine_pack(struct gmachine *g, size_t n, int8_t t);
void gmachine_split(struct gmachine *g, size_t n);
struct node_base *gmachine_track(struct gmachine *g, struct node_base *b);
void gmachine_gc(struct gmachine *g);

#endif // RUNTIME_H_
