#include "../include/runtime.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

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

  printf("Result: ");
  print_node(result);
  putchar('\n');
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
