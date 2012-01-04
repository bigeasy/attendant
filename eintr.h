/* With signal handlers in place, system calls are subject to interrupt. We need
 * to check all system calls for interrupts. This macro makes that less onerous.
 * None of the pthreads functions have the EINTR problem.
 */
#define HANDLE_EINTR(call, err)         \
do {                                    \
  err = (call);                         \
} while (err == -1 && errno == EINTR)
