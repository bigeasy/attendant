/* Many system calls are subject to interrupt when signal handlers are in place.
 * This macro makes that less onerous. None of the pthreads functions have the
 * `EINTR` problem.
 */
#define HANDLE_EINTR(call, err)         \
do {                                    \
  err = (call);                         \
} while (err == -1 && errno == EINTR)
