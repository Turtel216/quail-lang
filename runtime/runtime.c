#include "runtime.h"
#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Compile-time sanity: node_base must be exactly 2 words (16 bytes). */
_Static_assert(sizeof(struct node_base) == 16,
               "node_base must be 16 bytes (header + gc_next)");

extern void f_main(struct gmachine *s);

/* =========================================================================
 * Minor Heap -- Bump Allocator
 * =========================================================================
 *
 * The minor heap is a contiguous region of memory.  Allocation proceeds
 * from high addresses to low addresses (OCaml style):
 *
 *   start                       top                              limit
 *     |       free space         |        allocated objects        |
 *     v                          v                                 v
 *     [__________________________|+++++++++++++++++++++++++++++++++++]
 *
 * minor_alloc(g, size) decrements `top` by `size` and returns the new `top`.
 * When there is not enough room (new_top < start), a minor GC is triggered
 * to promote surviving objects to the major heap, then the heap is reset.
 * ========================================================================= */

/* Check whether a pointer falls within the minor heap region. */
static inline int in_minor_heap(struct gmachine *g, struct node_base *n) {
  uint8_t *p = (uint8_t *)n;
  return p >= g->minor_heap.start && p < g->minor_heap.limit;
}

/* Bump-allocate `size_bytes` from the minor heap.
 * Returns a zeroed-header node_base pointer.  May trigger minor_gc. */
static struct node_base *minor_alloc(struct gmachine *g, size_t size_bytes) {
  assert(size_bytes >= sizeof(struct node_base));
  assert(size_bytes % sizeof(uint64_t) == 0);

  uint8_t *new_top = g->minor_heap.top - size_bytes;
  if (new_top < g->minor_heap.start) {
    minor_gc(g);
    new_top = g->minor_heap.top - size_bytes;
    assert(new_top >= g->minor_heap.start &&
           "minor heap overflow: single object larger than heap");
  }

  g->minor_heap.top = new_top;
  struct node_base *node = (struct node_base *)new_top;
  node->header = 0;
  node->gc_next = NULL;
  return node;
}

/* =========================================================================
 * Allocation functions
 *
 * Each allocator bump-allocates from the minor heap.  Arguments that are
 * GC-managed pointers (could be minor-heap objects) must be pushed onto the
 * stack before calling minor_alloc, because a minor GC inside minor_alloc
 * may relocate them.  After minor_alloc returns, we pop the (possibly
 * updated) values back.
 * ========================================================================= */

struct node_app *alloc_app(struct gmachine *g, struct node_base *l,
                           struct node_base *r) {
  /* Protect l and r: they may point into the minor heap and a GC inside
   * minor_alloc would move them.  Pushing them onto the stack ensures the
   * GC can find and update them. */
  stack_push(&g->stack, l);
  stack_push(&g->stack, r);

  struct node_app *node =
      (struct node_app *)minor_alloc(g, sizeof(struct node_app));

  // Pop back, values may have been updated by a minor GC.
  r = stack_pop(&g->stack);
  l = stack_pop(&g->stack);

  node->base.header = MAKE_HEADER(NODE_APP, GC_WHITE, WORDS_NODE_APP);
  node->left = l;
  node->right = r;
  return node;
}

struct node_global *alloc_global(struct gmachine *g,
                                 void (*f)(struct gmachine *), int32_t a) {
  /* f is a C function pointer and a is an int, neither is a GC-managed
   * pointer, so no stack protection is needed. */
  struct node_global *node =
      (struct node_global *)minor_alloc(g, sizeof(struct node_global));
  node->base.header = MAKE_HEADER(NODE_GLOBAL, GC_WHITE, WORDS_NODE_GLOBAL);
  node->arity = a;
  node->function = f;
  return node;
}

struct node_ind *alloc_ind(struct gmachine *g, struct node_base *n) {
  /* Protect n if it is a GC-managed pointer. */
  int need_protect = (n != NULL && IS_PTR(n));
  if (need_protect)
    stack_push(&g->stack, n);

  struct node_ind *node =
      (struct node_ind *)minor_alloc(g, sizeof(struct node_ind));

  if (need_protect)
    n = stack_pop(&g->stack);

  node->base.header = MAKE_HEADER(NODE_IND, GC_WHITE, WORDS_NODE_IND);
  node->next = n;
  return node;
}

/* =========================================================================
 * Remembered Set -- tracks cross-generational pointers
 *
 * When a major-heap object is mutated to point into the minor heap
 * (e.g. gmachine_update rewrites a promoted node), we record it here.
 * During minor GC these entries are treated as additional roots so
 * their minor-heap children get evacuated.
 * ========================================================================= */

static void remembered_set_init(struct remembered_set *rs) {
  rs->capacity = 16;
  rs->count = 0;
  rs->data = malloc(sizeof(struct node_base *) * rs->capacity);
  assert(rs->data != NULL);
}

static void remembered_set_add(struct remembered_set *rs,
                               struct node_base *n) {
  if (rs->count >= rs->capacity) {
    rs->capacity *= 2;
    rs->data = realloc(rs->data, sizeof(struct node_base *) * rs->capacity);
    assert(rs->data != NULL);
  }
  rs->data[rs->count++] = n;
}

static void remembered_set_free(struct remembered_set *rs) {
  free(rs->data);
  rs->data = NULL;
  rs->count = 0;
}

/* =========================================================================
 * Minor GC -- Cheney's Stop-and-Copy Algorithm
 * =========================================================================
 *
 * Promotes surviving minor-heap objects to the major heap (malloc).
 * Uses Cheney's algorithm: a single-pass BFS over the live object graph
 * with no recursion and no auxiliary mark stack.
 *
 * Terminology (standard Cheney):
 *   evacuate  -- copy a single object from minor heap to major heap,
 *                leave a forwarding pointer behind, and append the
 *                copy to the scan queue.
 *   scavenge  -- scan one evacuated object's child pointers and
 *                evacuate any that still point into the minor heap.
 *   scan queue -- explicit FIFO that serves the role of Cheney's
 *                 "to-space scan pointer".  Since promoted objects
 *                 are malloc'd (non-contiguous), we cannot use a
 *                 linear scan pointer; an array-based queue gives
 *                 the same O(N) single-pass behaviour.
 *
 * Algorithm:
 *   1. Evacuate roots (stack entries pointing into the minor heap).
 *   2. While the scan pointer has not caught up with the queue tail,
 *      scavenge the next evacuated object.
 *   3. Free the arrays of dead (non-forwarded) NODE_DATA objects
 *      by walking the minor heap linearly.
 *   4. Reset the minor heap (top = limit).
 *   5. Optionally trigger a major GC if the threshold is exceeded.
 * ========================================================================= */

/* -- Scan Queue (Cheney worklist) ----------------------------------------- */

struct scan_queue {
  struct node_base **data;
  size_t count;    /* next free slot (Cheney "free pointer") */
  size_t scan;     /* next object to scavenge (Cheney "scan pointer") */
  size_t capacity;
};

static void queue_init(struct scan_queue *q) {
  q->capacity = 64;
  q->count = 0;
  q->scan = 0;
  q->data = malloc(sizeof(struct node_base *) * q->capacity);
  assert(q->data != NULL);
}

static void queue_push(struct scan_queue *q, struct node_base *n) {
  if (q->count >= q->capacity) {
    q->capacity *= 2;
    q->data = realloc(q->data, sizeof(struct node_base *) * q->capacity);
    assert(q->data != NULL);
  }
  q->data[q->count++] = n;
}

static void queue_free(struct scan_queue *q) {
  free(q->data);
  q->data = NULL;
}

/* -- Evacuate ------------------------------------------------------------- */

/* Copy a minor-heap object to the major heap (malloc), leave a forwarding
 * pointer in the old location, and append the copy to the scan queue.
 * Returns the new address (or the original value for ints, major-heap
 * pointers, and already-forwarded objects). */
static struct node_base *evacuate(struct gmachine *g, struct node_base *n,
                                  struct scan_queue *q) {
  if (IS_INT(n))
    return n;
  if (!in_minor_heap(g, n))
    return n;
  if (HDR_IS_FWD(n->header))
    return HDR_FWD(n->header);

  /* Allocate a copy in the major heap. */
  size_t size = HDR_SIZE(n->header) * sizeof(uint64_t);
  assert(size > 0 && "evacuating object with zero size");
  struct node_base *copy = malloc(size);
  assert(copy != NULL);
  memcpy(copy, n, size);

  /* Prepend to the major heap linked list. */
  copy->gc_next = g->gc_nodes;
  g->gc_nodes = copy;
  g->gc_node_count++;

  /* Leave a forwarding pointer in the old minor-heap location.
   * The low 16 bits (tag, color, dtag, size) are preserved so that
   * the linear sweep in step 3 can still read HDR_SIZE. */
  n->header = HDR_SET_FWD(n->header, copy);

  /* Append to the scan queue -- will be scavenged later. */
  queue_push(q, copy);

  return copy;
}

/* -- Scavenge ------------------------------------------------------------- */

/* Scan one evacuated object's child pointers and evacuate any that
 * still point into the minor heap. */
static void scavenge(struct gmachine *g, struct node_base *node,
                     struct scan_queue *q) {
  enum node_tag tag = HDR_TAG(node->header);
  if (tag == NODE_APP) {
    struct node_app *app = (struct node_app *)node;
    app->left = evacuate(g, app->left, q);
    app->right = evacuate(g, app->right, q);
  } else if (tag == NODE_IND) {
    struct node_ind *ind = (struct node_ind *)node;
    ind->next = evacuate(g, ind->next, q);
  } else if (tag == NODE_DATA) {
    struct node_data *data = (struct node_data *)node;
    for (struct node_base **p = data->array; *p; p++) {
      *p = evacuate(g, *p, q);
    }
  }
  /* NODE_GLOBAL has no GC-managed children -- nothing to scavenge. */
}

/* -- Forward declarations for incremental major GC ----------------------- */
static void darken_children(struct gmachine *g, struct node_base *node);
static void gc_start_cycle(struct gmachine *g);
static void gc_slice(struct gmachine *g);

/* -- Minor GC entry point ------------------------------------------------- */

void minor_gc(struct gmachine *g) {
  struct scan_queue queue;
  queue_init(&queue);

  /* Step 1: Evacuate roots -- every stack entry pointing into the
   * minor heap gets copied to the major heap. */
  for (size_t i = 0; i < g->stack.count; i++) {
    g->stack.data[i] = evacuate(g, g->stack.data[i], &queue);
  }

  /* Step 2: Scavenge remembered set entries -- major-heap objects that
   * were mutated to point into the minor heap (write barrier).  Their
   * child pointers are updated in-place via evacuate. */
  for (size_t i = 0; i < g->remembered_set.count; i++) {
    scavenge(g, g->remembered_set.data[i], &queue);
  }
  g->remembered_set.count = 0; /* clear -- all cross-gen refs resolved */

  /* Step 3: Cheney scan loop.  Process evacuated objects in FIFO order.
   * Each scavenge may evacuate more objects (appended to queue.count),
   * so the loop naturally terminates when all transitive references
   * have been processed.  This is a single O(N) pass. */
  while (queue.scan < queue.count) {
    scavenge(g, queue.data[queue.scan], &queue);
    queue.scan++;
  }

  /* Step 4: If an incremental major GC cycle is active, protect promoted
   * objects and their major-heap children from being incorrectly swept.
   *   - During MARK: promoted objects are BLACK; WHITE major-heap children
   *     are pushed GREY onto the mark stack so the marker will trace them.
   *   - During SWEEP: promoted objects are BLACK; WHITE major-heap children
   *     are also forced BLACK to survive the current sweep pass. */
  if (g->gc_state.phase != GC_IDLE) {
    for (size_t i = 0; i < queue.count; i++) {
      struct node_base *obj = queue.data[i];
      obj->header = HDR_SET_COLOR(obj->header, GC_BLACK);
      darken_children(g, obj);
    }
  }

  queue_free(&queue);

  /* Step 5: Free the arrays of dead (non-forwarded) NODE_DATA objects.
   * Walk the minor heap linearly from top (lowest alloc) to limit (end).
   * Each object's size is read from HDR_SIZE (preserved even after
   * forwarding was set in the upper bits). */
  {
    uint8_t *p = g->minor_heap.top;
    while (p < g->minor_heap.limit) {
      struct node_base *obj = (struct node_base *)p;
      size_t obj_size = HDR_SIZE(obj->header) * sizeof(uint64_t);
      assert(obj_size > 0 && "zero-size object in minor heap");

      if (!HDR_IS_FWD(obj->header) && HDR_TAG(obj->header) == NODE_DATA) {
        struct node_data *data = (struct node_data *)obj;
        free(data->array);
      }

      p += obj_size;
    }
  }

  /* Step 6: Reset the minor heap -- all live objects have been promoted. */
  g->minor_heap.top = g->minor_heap.limit;

  /* Step 7: If no major GC cycle is active and the threshold is exceeded,
   * start a new incremental cycle.  Then do one slice of work regardless. */
  if (g->gc_state.phase == GC_IDLE &&
      g->gc_node_count >= g->gc_node_threshold) {
    gc_start_cycle(g);
  }
  if (g->gc_state.phase != GC_IDLE) {
    gc_slice(g);
  }
}


/* =========================================================================
 * Incremental Major GC -- Tri-Color Mark-and-Sweep
 * =========================================================================
 *
 * The major GC runs in bounded-work slices, called once per minor GC.
 * A full cycle has two phases:
 *
 *   MARK  -- Pop objects from the mark stack (grey set), trace their
 *            children (push WHITE children as GREY), mark them BLACK.
 *            When the mark stack is empty, transition to SWEEP.
 *
 *   SWEEP -- Walk the gc_nodes list with a cursor.  Free WHITE objects
 *            (unreachable).  Reset BLACK objects to WHITE (ready for
 *            the next cycle).  When the cursor reaches the end,
 *            transition to IDLE.
 *
 * Correctness between slices is maintained by:
 *   - Insertion barrier in gmachine_update:  if a BLACK object is
 *     mutated during MARK phase, push it GREY for rescanning.
 *   - Darkening in minor_gc:  promoted objects are BLACK; their WHITE
 *     major-heap children are greyed (MARK) or blackened (SWEEP).
 * ========================================================================= */

#define MARK_SLICE_SIZE  256  /* objects per mark slice */
#define SWEEP_SLICE_SIZE 256  /* objects per sweep slice */

/* Free any external resources owned by a node (e.g. node_data's array). */
static void free_node_direct(struct node_base *n) {
  if (HDR_TAG(n->header) == NODE_DATA) {
    free(((struct node_data *)n)->array);
  }
}

/* -- GC State Lifecycle --------------------------------------------------- */

static void gc_state_init(struct gc_state *s) {
  s->phase = GC_IDLE;
  s->mark_stack = NULL;
  s->mark_count = 0;
  s->mark_capacity = 0;
  s->sweep_ptr = NULL;
}

static void gc_state_free(struct gc_state *s) {
  free(s->mark_stack);
  s->mark_stack = NULL;
  s->mark_count = 0;
  s->mark_capacity = 0;
}

/* -- Mark Stack ----------------------------------------------------------- */

/* Push a node onto the mark stack and set its color to GREY. */
static void gc_mark_push(struct gc_state *s, struct node_base *n) {
  if (s->mark_count >= s->mark_capacity) {
    s->mark_capacity = s->mark_capacity ? s->mark_capacity * 2 : 64;
    s->mark_stack =
        realloc(s->mark_stack, sizeof(struct node_base *) * s->mark_capacity);
    assert(s->mark_stack != NULL);
  }
  n->header = HDR_SET_COLOR(n->header, GC_GREY);
  s->mark_stack[s->mark_count++] = n;
}

/* -- Darken --------------------------------------------------------------- */

/* Ensure a major-heap object is at least GREY during an active GC cycle.
 * Called when a new reference to the object is discovered outside the
 * normal mark traversal (e.g. via minor GC promotion or write barrier).
 *
 *   MARK phase  -> mark GREY and push onto mark stack (will be traced).
 *   SWEEP phase -> force BLACK (survives current sweep pass). */
static void darken(struct gmachine *g, struct node_base *n) {
  if (IS_INT(n))
    return;
  if (in_minor_heap(g, n))
    return;
  if (HDR_COLOR(n->header) != GC_WHITE)
    return;

  if (g->gc_state.phase == GC_MARK) {
    gc_mark_push(&g->gc_state, n);
  } else if (g->gc_state.phase == GC_SWEEP) {
    n->header = HDR_SET_COLOR(n->header, GC_BLACK);
  }
}

/* Darken all GC-managed children of a node. */
static void darken_children(struct gmachine *g, struct node_base *node) {
  enum node_tag tag = HDR_TAG(node->header);
  if (tag == NODE_APP) {
    struct node_app *app = (struct node_app *)node;
    darken(g, app->left);
    darken(g, app->right);
  } else if (tag == NODE_IND) {
    struct node_ind *ind = (struct node_ind *)node;
    darken(g, ind->next);
  } else if (tag == NODE_DATA) {
    struct node_data *data = (struct node_data *)node;
    for (struct node_base **p = data->array; *p; p++) {
      darken(g, *p);
    }
  }
  /* NODE_GLOBAL has no GC-managed children. */
}

/* -- Cycle Start ---------------------------------------------------------- */

/* Begin a new mark-sweep cycle.  Push all stack roots as GREY. */
static void gc_start_cycle(struct gmachine *g) {
  assert(g->gc_state.phase == GC_IDLE);
  g->gc_state.phase = GC_MARK;

  for (size_t i = 0; i < g->stack.count; i++) {
    struct node_base *n = g->stack.data[i];
    if (IS_PTR(n) && HDR_COLOR(n->header) == GC_WHITE) {
      gc_mark_push(&g->gc_state, n);
    }
  }
}

/* -- Mark Slice ----------------------------------------------------------- */

/* Process up to MARK_SLICE_SIZE objects from the mark stack.
 * For each object: mark BLACK, trace children (push WHITE ones GREY).
 * When the mark stack empties, transition to SWEEP. */
static void gc_mark_slice(struct gmachine *g) {
  struct gc_state *s = &g->gc_state;
  size_t work = 0;

  while (s->mark_count > 0 && work < MARK_SLICE_SIZE) {
    struct node_base *n = s->mark_stack[--s->mark_count];
    n->header = HDR_SET_COLOR(n->header, GC_BLACK);
    work++;

    enum node_tag tag = HDR_TAG(n->header);
    if (tag == NODE_APP) {
      struct node_app *app = (struct node_app *)n;
      if (IS_PTR(app->left) && HDR_COLOR(app->left->header) == GC_WHITE)
        gc_mark_push(s, app->left);
      if (IS_PTR(app->right) && HDR_COLOR(app->right->header) == GC_WHITE)
        gc_mark_push(s, app->right);
    } else if (tag == NODE_IND) {
      struct node_ind *ind = (struct node_ind *)n;
      if (IS_PTR(ind->next) && HDR_COLOR(ind->next->header) == GC_WHITE)
        gc_mark_push(s, ind->next);
    } else if (tag == NODE_DATA) {
      struct node_data *data = (struct node_data *)n;
      for (struct node_base **p = data->array; *p; p++) {
        if (IS_PTR(*p) && HDR_COLOR((*p)->header) == GC_WHITE)
          gc_mark_push(s, *p);
      }
    }
    /* NODE_GLOBAL: no children. */
  }

  /* If mark stack is empty, all reachable objects are BLACK.
   * Transition to sweep phase. */
  if (s->mark_count == 0) {
    s->phase = GC_SWEEP;
    s->sweep_ptr = &g->gc_nodes;
  }
}

/* -- Sweep Slice ---------------------------------------------------------- */

/* Process up to SWEEP_SLICE_SIZE nodes from the gc_nodes list.
 * WHITE -> free (garbage).  BLACK -> reset to WHITE (survivor). */
static void gc_sweep_slice(struct gmachine *g) {
  struct gc_state *s = &g->gc_state;
  size_t work = 0;

  while (*s->sweep_ptr != NULL && work < SWEEP_SLICE_SIZE) {
    struct node_base *n = *s->sweep_ptr;

    if (HDR_COLOR(n->header) == GC_BLACK) {
      /* Survived -- reset to WHITE for the next cycle. */
      n->header = HDR_SET_COLOR(n->header, GC_WHITE);
      s->sweep_ptr = &n->gc_next;
    } else {
      /* Unreachable (WHITE or stale GREY) -- free it. */
      *s->sweep_ptr = n->gc_next;
      free_node_direct(n);
      free(n);
      g->gc_node_count--;
    }
    work++;
  }

  /* If we reached the end of the list, the cycle is complete. */
  if (*s->sweep_ptr == NULL) {
    s->phase = GC_IDLE;
    g->gc_node_threshold = g->gc_node_count * 2;
    if (g->gc_node_threshold < 128)
      g->gc_node_threshold = 128;
  }
}

/* -- Slice Dispatch ------------------------------------------------------- */

/* Do one bounded slice of major GC work (mark or sweep). */
static void gc_slice(struct gmachine *g) {
  if (g->gc_state.phase == GC_MARK) {
    gc_mark_slice(g);
  } else if (g->gc_state.phase == GC_SWEEP) {
    gc_sweep_slice(g);
  }
}

/* -- Full GC (force complete cycle) --------------------------------------- */

/* Run an entire mark-sweep cycle to completion.  Useful for shutdown
 * or debugging.  Normal runtime uses gc_slice for incremental work. */
void gmachine_gc(struct gmachine *g) {
  if (g->gc_state.phase == GC_IDLE) {
    gc_start_cycle(g);
  }
  while (g->gc_state.phase != GC_IDLE) {
    gc_slice(g);
  }
}

// === Stack ===

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

// === G-Machine ===

void gmachine_init(struct gmachine *g) {
  stack_init(&g->stack);

  /* Allocate the minor heap region. */
  g->minor_heap.start = malloc(MINOR_HEAP_SIZE);
  assert(g->minor_heap.start != NULL && "failed to allocate minor heap");
  g->minor_heap.limit = g->minor_heap.start + MINOR_HEAP_SIZE;
  g->minor_heap.top = g->minor_heap.limit; /* empty -- grows downward */

  remembered_set_init(&g->remembered_set);
  gc_state_init(&g->gc_state);

  g->gc_nodes = NULL;
  g->gc_node_count = 0;
  g->gc_node_threshold = 128;
}

void gmachine_free(struct gmachine *g) {
  stack_free(&g->stack);

  /* Free the minor heap region. */
  free(g->minor_heap.start);
  g->minor_heap.start = NULL;
  g->minor_heap.top = NULL;
  g->minor_heap.limit = NULL;

  remembered_set_free(&g->remembered_set);
  gc_state_free(&g->gc_state);

  /* Free all major heap objects. */
  struct node_base *to_free = g->gc_nodes;
  struct node_base *next;
  while (to_free) {
    next = to_free->gc_next;
    free_node_direct(to_free);
    free(to_free);
    to_free = next;
  }
}

void gmachine_slide(struct gmachine *g, size_t n) {
  assert(g->stack.count > n);
  g->stack.data[g->stack.count - n - 1] = g->stack.data[g->stack.count - 1];
  g->stack.count -= n;
}

void gmachine_update(struct gmachine *g, size_t o) {
  assert(g->stack.count > o + 1);
  struct node_ind *ind =
      (struct node_ind *)g->stack.data[g->stack.count - o - 2];

  /* Save the old GC color before rewriting the header. */
  enum gc_color old_color = HDR_COLOR(ind->base.header);

  /* Rewrite the node as an indirection, preserving the GC color. */
  ind->base.header = MAKE_HEADER(NODE_IND, old_color, WORDS_NODE_IND);
  ind->next = g->stack.data[g->stack.count -= 1];

  /* Write barrier (generational): if a major-heap object now points into
   * the minor heap, record it in the remembered set so the next minor GC
   * can find and evacuate the target. */
  if (!in_minor_heap(g, (struct node_base *)ind) &&
      IS_PTR(ind->next) && in_minor_heap(g, ind->next)) {
    remembered_set_add(&g->remembered_set, (struct node_base *)ind);
  }

  /* Write barrier (incremental): if we are in the MARK phase and the
   * mutated object is BLACK (already traced), push it back as GREY so
   * the marker will rescan it and discover the new child ind->next.
   * This is an insertion barrier (Dijkstra-style). */
  if (g->gc_state.phase == GC_MARK && old_color == GC_BLACK &&
      !in_minor_heap(g, (struct node_base *)ind)) {
    gc_mark_push(&g->gc_state, (struct node_base *)ind);
  }
}

void gmachine_alloc(struct gmachine *g, size_t o) {
  while (o--) {
    stack_push(&g->stack, (struct node_base *)alloc_ind(g, NULL));
  }
}

void gmachine_pack(struct gmachine *g, size_t n, int8_t t) {
  assert(g->stack.count >= n);

  /* Allocate the node_data FIRST (may trigger minor GC, which updates
   * stack entries).  Then copy from the stack so we get the post-GC
   * pointers. */
  struct node_data *new_node =
      (struct node_data *)minor_alloc(g, sizeof(struct node_data));
  new_node->base.header = MAKE_HEADER_DATA(GC_WHITE, t, WORDS_NODE_DATA);

  struct node_base **data = malloc(sizeof(*data) * (n + 1));
  assert(data != NULL);
  memcpy(data, &g->stack.data[g->stack.count - n], n * sizeof(*data));
  data[n] = NULL;
  new_node->array = data;

  stack_popn(&g->stack, n);
  stack_push(&g->stack, (struct node_base *)new_node);
}

void gmachine_split(struct gmachine *g, size_t n) {
  struct node_data *node = (struct node_data *)stack_pop(&g->stack);
  for (size_t i = 0; i < n; i++) {
    stack_push(&g->stack, node->array[i]);
  }
}

// === Evaluation ===

void unwind(struct gmachine *g) {
  struct stack *s = &g->stack;

  while (1) {
    struct node_base *peek = stack_peek(s, 0);

    // A tagged integer is already in WHNF, stop unwinding.
    if (IS_INT(peek))
      break;

    enum node_tag tag = HDR_TAG(peek->header);
    if (tag == NODE_APP) {
      struct node_app *n = (struct node_app *)peek;
      stack_push(s, n->left);
    } else if (tag == NODE_GLOBAL) {
      struct node_global *n = (struct node_global *)peek;
      assert(s->count > n->arity);

      for (size_t i = 1; i <= n->arity; i++) {
        s->data[s->count - i] =
            ((struct node_app *)s->data[s->count - i - 1])->right;
      }

      n->function(g);
    } else if (tag == NODE_IND) {
      struct node_ind *n = (struct node_ind *)peek;
      stack_pop(s);
      stack_push(s, n->next);
    } else {
      break;
    }
  }
}

void print_node(struct node_base *n) {
  // Tagged integer, print directly, no heap access.
  if (IS_INT(n)) {
    printf("%ld", VAL_INT(n));
    return;
  }

  enum node_tag tag = HDR_TAG(n->header);
  if (tag == NODE_APP) {
    struct node_app *app = (struct node_app *)n;
    print_node(app->left);
    putchar(' ');
    print_node(app->right);
  } else if (tag == NODE_DATA) {
    printf("(Packed)");
  } else if (tag == NODE_GLOBAL) {
    struct node_global *global = (struct node_global *)n;
    printf("(Global: %p)", global->function);
  } else if (tag == NODE_IND) {
    print_node(((struct node_ind *)n)->next);
  }
}

int main(int argc, char **argv) {
  struct gmachine gmachine;

  // Must init BEFORE allocating, alloc_global needs the minor heap.
  gmachine_init(&gmachine);

  struct node_global *first_node = alloc_global(&gmachine, f_main, 0);
  struct node_base *result;

  stack_push(&gmachine.stack, (struct node_base *)first_node);
  unwind(&gmachine);
  result = stack_pop(&gmachine.stack);
  printf("Result: ");
  print_node(result);
  putchar('\n');
  gmachine_free(&gmachine);
}
