#include "../include/runtime.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void f_main(struct stack *s);

struct node_base *eval(struct node_base *n);

void print_node(struct node_base *n) {
  if (n->tag == NODE_APP) {
    struct node_app *app = (struct node_app *)n;
    print_node(app->left);
    putchar(' ');
    print_node(app->right);
  } else if (n->tag == NODE_DATA) {
    printf("(Packed)");
  } else if (n->tag == NODE_GLOBAL) {
    struct node_global *global = (struct node_global *)n;
    printf("(Global: %p)", global->function);
  } else if (n->tag == NODE_IND) {
    print_node(((struct node_ind *)n)->next);
  } else if (n->tag == NODE_NUM) {
    struct node_num *num = (struct node_num *)n;
    printf("%d", num->value);
  }
}

int main(int argc, char **argv) {
  struct node_global *first_node = alloc_global(f_main, 0);
  struct node_base *result = eval((struct node_base *)first_node);
}

void unwind(struct stack *s) {
  while (1) {
    struct node_base *peek = stack_peek(s, 0);
    if (peek->tag == NODE_APP) {
      struct node_app *n = (struct node_app *)peek;
      stack_push(s, n->left);
    } else if (peek->tag == NODE_GLOBAL) {
      struct node_global *n = (struct node_global *)peek;
      assert(s->count > n->arity);

      for (size_t i = 1; i <= n->arity; i++) {
        s->data[s->count - i] =
            ((struct node_app *)s->data[s->count - i - 1])->right;
      }

      n->function(s);
    } else if (peek->tag == NODE_IND) {
      struct node_ind *n = (struct node_ind *)peek;
      stack_pop(s);
      stack_push(s, n->next);
    } else {
      break;
    }
  }
}

struct node_base *eval(struct node_base *n) {
  struct stack program_stack;
  stack_init(&program_stack);
  stack_push(&program_stack, n);
  unwind(&program_stack);
  struct node_base *result = stack_pop(&program_stack);
  stack_free(&program_stack);

  printf("Result: ");
  print_node(result);
  putchar('\n');

  return result;
}

struct node_base *alloc_node() {
  struct node_base *new_node = malloc(sizeof(struct node_app));
  assert(new_node != NULL);
  return new_node;
}

struct node_app *alloc_app(struct node_base *l, struct node_base *r) {
  struct node_app *node = (struct node_app *)alloc_node();
  node->base.tag = NODE_APP;
  node->left = l;
  node->right = r;
  return node;
}

struct node_num *alloc_num(int32_t n) {
  struct node_num *node = (struct node_num *)alloc_node();
  node->base.tag = NODE_NUM;
  node->value = n;
  return node;
}

struct node_global *alloc_global(void (*f)(struct stack *), int32_t a) {
  struct node_global *node = (struct node_global *)alloc_node();
  node->base.tag = NODE_GLOBAL;
  node->arity = a;
  node->function = f;
  return node;
}

struct node_ind *alloc_ind(struct node_base *n) {
  struct node_ind *node = (struct node_ind *)alloc_node();
  node->base.tag = NODE_IND;
  node->next = n;
  return node;
}

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
  memcpy(data, &s->data[s->count - n], n * sizeof(*data));

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
