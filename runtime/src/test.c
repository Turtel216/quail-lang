#include "../include/runtime.h"

void f_add(struct stack *s) {
  struct node_num *left = (struct node_num *)eval(stack_peek(s, 0));
  struct node_num *right = (struct node_num *)eval(stack_peek(s, 1));
  stack_push(s, (struct node_base *)alloc_num(left->value + right->value));
}

void f_main(struct stack *s) {
  // PushInt 320
  stack_push(s, (struct node_base *)alloc_num(320));

  // PushInt 6
  stack_push(s, (struct node_base *)alloc_num(6));

  // PushGlobal f_add (the function for +)
  stack_push(s, (struct node_base *)alloc_global(f_add, 2));

  struct node_base *left;
  struct node_base *right;

  // MkApp
  left = stack_pop(s);
  right = stack_pop(s);
  stack_push(s, (struct node_base *)alloc_app(left, right));

  // MkApp
  left = stack_pop(s);
  right = stack_pop(s);
  stack_push(s, (struct node_base *)alloc_app(left, right));
}
