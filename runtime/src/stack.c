#include "../include/runtime.h"

#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>

void stack_init(struct stack *s) {
  s->size = 4;
  s->count = 0;
  s->data = malloc(sizeof(*s->data) * s->size);
  assert(s->data != NULL);
}

void stack_free(struct stack *s) { free(s->data); }

void stack_push(struct stack *s, struct node_base *n) {
  while (s->count >= s->size) {
    s->data = realloc(s->data, sizeof(*s->data) * (s->size *= 2));
    assert(s->data != NULL);
  }
  s->data[s->count++] = n;
}

struct node_base *stack_pop(struct stack *s) {
  assert(s->count > 0);
  return s->data[--s->count];
}

struct node_base *stack_peek(struct stack *s, size_t o) {
  assert(s->count > o);
  return s->data[s->count - o - 1];
}

void stack_popn(struct stack *s, size_t n) {
  assert(s->count >= n);
  s->count -= n;
}

void stack_slide(struct stack *s, size_t n) {
  assert(s->count > n);
  s->data[s->count - n - 1] = s->data[s->count - 1];
  s->count -= n;
}

void stack_update(struct stack *s, size_t o) {
  assert(s->count > o + 1);
  struct node_ind *ind = (struct node_ind *)s->data[s->count - o - 2];
  ind->base.tag = NODE_IND;
  ind->next = s->data[s->count -= 1];
}

void stack_alloc(struct stack *s, size_t o) {
  while (o--) {
    stack_push(s, (struct node_base *)alloc_ind(NULL));
  }
}

void stack_pack(struct stack *s, size_t n, int8_t t) {
  assert(s->count >= n);

  struct node_base **data = malloc(sizeof(*data) * n);
  assert(data != NULL);
  memcpy(data, &s->data[s->count - 1 - n], n * sizeof(*data));

  struct node_data *new_node = (struct node_data *)alloc_node();
  new_node->array = data;
  new_node->base.tag = NODE_DATA;
  new_node->tag = t;

  stack_popn(s, n);
  stack_push(s, (struct node_base *)new_node);
}

void stack_split(struct stack *s, size_t n) {
  struct node_data *node = (struct node_data *)stack_pop(s);
  for (size_t i = 0; i < n; i++) {
    stack_push(s, node->array[i]);
  }
}
