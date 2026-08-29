#ifndef QUAIL_RT_PANIC_H_
#define QUAIL_RT_PANIC_H_

/* -- Fatal errors ----------------------------------------------------------
 *
 * The runtime has no way to propagate an allocation failure back to the
 * generated code: a G-machine reduction step cannot fail gracefully.  When
 * memory runs out or an invariant is violated we print a diagnostic and
 * abort.
 *
 * These are deliberately not assertions.  `assert` compiles away under
 * -DNDEBUG (the release build), which would turn an out-of-memory condition
 * into a NULL dereference.
 */

/* Print `msg` to stderr and abort.  Never returns. */
_Noreturn void rt_fatal(const char *msg);

/* Report an allocation failure for `what` and abort.  Never returns. */
_Noreturn void rt_oom(const char *what);

#endif /* QUAIL_RT_PANIC_H_ */
