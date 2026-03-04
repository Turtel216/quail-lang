#ifndef RUNTIME_H_
#define RUNTIME_H_

#include <stddef.h>
#include <stdint.h>

struct stack;

enum node_tag { NODE_APP, NODE_NUM, NODE_GLOBAL, NODE_IND, NODE_DATA };

struct node_base {
  enum node_tag tag;
};

struct node_app {
  struct node_base base;
  struct node_base *left;
  struct node_base *right;
};

struct node_num {
  struct node_base base;
  int32_t value;
};

struct node_global {
  struct node_base base;
  int32_t arity;
  void (*function)(struct stack *);
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
struct node_num *alloc_num(int32_t n);
struct node_global *alloc_global(void (*f)(struct stack *), int32_t a);
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

struct node_base *eval(struct node_base *n);

#endif // RUNTIME_H_
