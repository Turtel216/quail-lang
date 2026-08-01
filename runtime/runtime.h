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
 *   LSB == 0  ->  pointer to a heap-allocated node
 */

#define IS_INT(v) ((uintptr_t)(v) & 1)
#define IS_PTR(v) (!IS_INT(v))

// Encode a C integer into a tagged value.  Shift left by 1 and set LSB.
#define MAKE_INT(n) ((struct node_base *)(((intptr_t)(n) << 1) | 1))

/* Decode a tagged value back to a C integer.  Arithmetic right-shift
 * preserves the sign bit. */
#define VAL_INT(v) ((int64_t)((intptr_t)(v) >> 1))

// Assert that a tagged value is a real pointer before dereferencing.
#define AS_PTR(v)                                                              \
  (assert(IS_PTR(v) && "expected heap pointer, got tagged int"), (v))

struct stack;

enum node_tag { NODE_APP, NODE_GLOBAL, NODE_IND, NODE_DATA };

struct gmachine;

struct node_base {
  enum node_tag tag;
  int8_t gc_reachable;
  struct node_base *gc_next;
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

struct node_data {
  struct node_base base;
  int8_t tag;
  struct node_base **array;
};

struct node_base *alloc_node();
struct node_app *alloc_app(struct node_base *l, struct node_base *r);
struct node_global *alloc_global(void (*f)(struct gmachine *), int32_t a);
struct node_ind *alloc_ind(struct node_base *n);

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
