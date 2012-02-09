/* 
 * A process monitor that watches a single out-of-process server on behalf of a
 * dynamically loaded plugin running within the context of a host application. 
 *
 * Attendant was written for use in an [NPAPI](https://wiki.mozilla.org/NPAPI)
 * plugin, but there is no NPAPI specific code here. I imagine that Attendant
 * could work with any similar plugin architecture, where a host application
 * loads a plugin that is implemented as dynamically linked library.
 *
 * Attendant allows a plugin to launch a single server process. With this single
 * server process you can implement your plugin as a proxy to an out-of-process
 * server that performs the real work of the plugin. It isolates the complexity
 * of the plugin in a separate process, so that catastrophic errors will kill
 * only the plugin server, and not the host application.
 *
 * Attendant is supposed to monitor only the single server process. The server
 * process will start when the library loads. The server process will shutdown
 * when the library is unloaded and cannot be restarted. If the server process
 * exits before shutdown, the exit is considered abnormal, and recovery ensues.
 *
 * This behavior is proscribed, intentional and not a limitation of the
 * Attendant's ability. It is a limitation of the Attendant's scope.
 *
 * If you need to launch multiple processes to service your plugin, then make
 * the first process you launch a process that monitors your flock of processes.
 * Attendant is not a general purpose process monitor, it focues on the
 * challenges of monitoring a single process in the context of a host
 * application that may or may not expect plugins to launch child processes. It
 * isolates the plugin server process from the host application and vice versa. 
 * 
 * The Attendant is only a process monitor. It does not provide a API for
 * communication between the plugin stub and the plugin process. 
 *
 * You can initialize inter-process communication using command line arguments
 * and the redirected standard I/O pipes. You can commicate using named pipes
 * or TCP/IP sockets.
 * 
 * You might decide that standard I/O is sufficient for inter-process
 * communication between your plugin stub and the plugin process.  You are still
 * responsible for desiging a protocol that will use the standard I/O pipes.
 *
 * Attendant may not work with all host applications, but it does its best to be
 * as unobtrusive as possible. It forks and execs calling only async-safe system
 * calls to dup the pipes that redirect stdio. It then execs an intermediate
 * program that closes file descriptors inherited form the host applications and
 * ensure that signal handers are returned to their default disposition. The
 * intermediate program then launches your plugin server program.
 *
 * The plugin API may be supported my numerous applications, meaning that your
 * plugin may be loaded into host applications with different architectures.
 * Attendant monitors your process even thought it might not be able to do
 * traditional process monitoring because the host application has employed
 * signals and is waiting on all child processes for its own process monitoring
 * needs.
 *
 * You might be in a multi-threaded application, where forked proceses can only
 * make async-safe system calls, anything less is thread-unsafe. You might be in
 * an multi-process application, that has registered its own signal handlers,
 * and treats your child process as one of its own workers.  You don't have
 * control over signal handlers. You don't know who's going to reap your child
 * process. You don't know what file descriptors your child process will
 * inherit.
 *
 * This required a lot of careful reading of man pages, so I've annotated the
 * code thurougly, in literate programming style, formatted for reading after
 * passing it though a fork of Docco I doctored to handle C's multi-line
 * comments. I'm going to remind myself now that the comments are supposed to
 * assist my recolection when the time comes to consider fork and exec once
 * again.
 *
 * ### Drop Off
 *
 * When we fork our server process, we are going to execve immediately. Assuming
 * the most fragile state possible, we will be sure not to make any system calls
 * that are not async-safe, allocate no memory, twiddle and mutexes.
 * 
 * At the time of writing, this code is indended for use on OS X, Linux and
 * Windows, to be dynamically linked into a running instance of Safari, Firefox
 * or Chrome. Here are the considerations for these environments.
 * 
 * Safari and Firefox have multi-threaded architectures, while Chrome has a
 * multi-process architecture. Fork and threads do not mix well. This creates
 * two sets of considerations.
 * 
 * For the multi-threaded architectures, fork can come as a surprise, distrubing
 * the state mutexes, stomping on the mutexes used to make traditional global
 * functions thread safe.
 * 
 * Chrome, on the other hand, is already launching processes. It has signal
 * handlers installed, and is waiting on process completion. It's attempts to
 * communicate with its child processes might be confused by additional child
 * processes that it didn't launch itself. Also, it might swallow signals that
 * we'll depend upon to monitor the server process.
 * 
 * The concerns raised by these architectures will be discussed and addressed in
 * the code below in the literate programming style.
 *
 * We're going to program defensively, to isolate our server process as quickly
 * as possible, as completely as possible. We're not up for the challenge of
 * 
 * There is a third possible architecture which is the multi-fubar architecture
 * that has done all that it can to create a fragile environment for monitoring
 * child processes. We'll attempt to address some of these issues as well.
 *
 * ### From Down Below
 * 
 * Mozilla has nsIProcess. This is a scriptable interface that allows add-on
 * developer to launch a process, so it is analogous to what we're doing here.
 * The Linux implementation of nsIProcess optionally establishes pipes for
 * stdio, but closes them if pipes are not desired, so that the forked process
 * does not inherit the browsers stdio handles. 
 *
 * Mozilla makes no attempt to close file any other handles. It has a an OS
 * abstraction library that sets optional FD_CLOEXEC on file handles that are
 * created based on an "inheritable" switch. Whether that switch is ever on is
 * difficult to determine, but it looks like no, so it would appear that file
 * handles are generally not inheritable in Mozilla, and closed on exec.
 *
 * Safari WebKit doesn't appear to call FD_CLOEXEC when it opens files or
 * sockets, so we might have a few lying around.
 *
 * Our multi-process architecture Google Chrome, might be caught off gaurd when
 * child processes are launched that are not its own, but it sets up the
 * environment for proper child handling. It chooses the file handles that a
 * child process will inherit carefully, but it seems like it does this right
 * before a fork, and all other handles are set to close on exec.
 *
 * Our third architecture is the unknown future multi-headache one where
 * there is grief is optimum. That would be a host application that has a
 * phenominal number of file handles open and ready to inherit.
 *
 * In any case, we're going to loop through all the open handles and close
 * the ones that are not stdio.
 *
 * The worst case is we have an enormous number of open file hanldes that we
 * have to close. The best case is that we have none. The error case is that
 * here are no handles left to get the handles we need to close the handles.
 *
 * TODO Occurs to me that we ought to insist that pipes are only used at
 * startup, to bootstrap a different form of IPC. We don't allow the pipes to be
 * used as the primary form of IPC. You can use the pipes to negotiate a TCP/IP
 * port or a named pipe, then use that for IPC. This means we can do something
 * to prevent `SIGPIPE`, plus, close the redirected standard I/O after the
 * bootstrapping is complete.
 *
 * We would move to a startup model that had a `starter` and a `connector`.
 *
 * Does this make it easier? Won't know until I integrate. Keeping standard I/O
 * around doesn't seem to be a big win.
 */

/* *Throw open Helicon now, goddesses, move your songs.* &mdash; Dante */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Local includes. */
#include "attendant.h"
#include "eintr.h"
#include "errors.h"

/* The abend handler will be called from a thread that is watching the process,
 * so if you supply a callback, be sure to be thread-safe.
 *
 * You are probably going to restart the server process, which means that you'll
 * have to reinitialize your library to use it. Any variables visibile to both
 * this function and your library need to be guarded by a mutex.
 *
 * And no, don't just make a variable volatile and think that will cover it. It
 * needs to be gaurded by a mutex to flush memory accross CPU cores.
 *
 * TODO Re-docco.
 */

/* &#9824; */
typedef void (*starter_t)(int restart);

/* TODO Document. */
typedef void (*connector_t)(attendant__pipe_t in, attendant__pipe_t out);

/* ### Global State
 *
 * We have a collection of conditions used to alert blocking stub functions of a
 * change in state in the server process.
 */

/* &#9824; */
struct cond {
  /* Server is running or it will never run again. */
  pthread_cond_t  running;  
  /* Server has shutdown. */
  pthread_cond_t  shutdown; 
  /* Used for sake of timed wait. */
  pthread_cond_t  getgpid;  
/* &mdash; */
};


/* For testing we keep a trace log. This is not a production log. If this is
 * enabled during production, tracing will stop when the array of poitns is
 * filled. */

/* */
#ifdef _DEBUG
struct trace {
  /* Each entry is a formatted string representing a trace point. */
  char *points[256];
  /* The current count of log entries. */
  int count;
  /* Gaurd for thread-safe traceing. */
  pthread_mutex_t mutex;      
};
#endif

/* The one and only process we watch. Static variables are gathered into this
 * structure so that when reading the code below, it is easy to see which are
 * static variables and which are local variables.*/

/* &#9824; */
struct process {
  /* Absolute path to the relay program. */
  char *relay;
  /* Arguments to pass to relay program, starting with the absolute path to the
   * plugin server program. */
  char **argv;
  /* The file descriptor inherited by server process that when closed indicates
   * that the server process has died. */
  int canary;
  /* User supplied server progress program start. */
  starter_t starter;
  /* User supplied plugin stub to server process IPC initialization. */
  connector_t connector;
  /* SIGCHLD is not SIG_IGN so waitpid will block on specific pid. */
  short waitable;
  /* The process pid. */
  pid_t pid;
  /* Server is running. */
  short running;
  /* The server is recovering from unexpected exit. */
  short restarting;
  /* Shutdown is pending and exit is expected. */
  short shutdown;
  /* A count of the number of times that the server has started and restarted.
   */
  int instance;
  /* The attendant error code and system error code for last thing that went
   * wrong. */
  struct attendant__errors errors;
  /* Thread local storage key for thread local storage of instance count. */
  pthread_key_t key;          
  /* Gaurd process variables referenced by both the stub functions running in
   * the plugin threads and the server process management threads. */
  pthread_mutex_t mutex;      
  /* Collection of conditions. */
  struct cond cond;
  /* The server process launcher thread. */
  pthread_t launcher;
  /* The server process reaper thread. */
  pthread_t reaper;
  /* We create seven pipes, so we create an array of seven pipe pairs. We then
   * refer to the pipes by name in code using the defines below that map the
   * pipe name to a pipe index. */
  attendant__pipe_t pipes[7][2];            
  /* Tracing variables. See above. */
#ifdef _DEBUG
  struct trace trace;
#endif
  /* &mdash; */
};

/* The variable structure for our once and only instance of the plugin
 * attendant. */

/* &mdash; */
static struct process process;

/* ### Pipes
 *
 * The collection of pipes in the process structure is indexed using constants.
 * There are seven pipes. Three are the expected pipes for stdio. Attendant
 * makes use of the EPIPE error to detect that a child process has exited, or
 * that a pipe with the `FD_CLOEXEC` flag set was closed on exec when exec was
 * called. This is how we monitor our child process, because `waitpid` might be
 * disabled by the host application.
 */

/* One pipe for each io stream.
 */

/* &mdash; */
#define PIPE_STDIN  0
#define PIPE_STDOUT 1
#define PIPE_STDERR 2

/* The fork pipe reports a successful fork initialization. We listen to the fork
 * pipe for an error number, or a zero, or report if it closes mysteriously.
 */

/* &mdash; */
#define PIPE_FORK   3

/* The exec pipe reports a successful exec. It guards against a race condition
 * where we've forked but not execed, and the client library calls scram, which
 * sends a kill SIGKILL, but not to our server process, but to the fork process.
 * Not the end of the world, but I'd rather send a SIGTERM to the fork process,
 * instead of SIGKILL.
 */

/* &mdash; */
#define PIPE_RELAY   4

/* The reaper thread will listen to the canary pipe, the other end is held by
 * the running child server process. When the pipe closes and we get an `EPIPE`
 * error, it means the plugin server process has exited. This is how we monitor
 * the child server process without relying on `waitpid` being functional. */

/* &mdash; */
#define PIPE_CANARY   5

/* The reaper thread will poll the canary pipe above, listening for the library
 * server process exit. At the same time it will poll this instance pipe, which
 * is used by the plugin stub to wake the reaper thread and tell it to forcibly
 * retart the plugin server process. The plugin server process may have become
 * unresponsive, but may not have exited. The plugin stub will detect this as
 * IPC calls timeout, while the reaper thread can only detect exit.
 */

/* &mdash; */
#define PIPE_REAPER   6

/* Process is one static structure, one process launched per library. It would
 * be easy enough to make this an API that has a handle, but if you did want to
 * run and watch a handful of server processes, it would better to make the
 * first process you launch that monitor, because you'll have complete control
 * over the environment, and your process monitoring strategy can take the shape
 * that best suits the needs of your application.
 *
 * All we're trying to do here is get one process cleanly spun off form the host
 * application. We don't want the host application monitoring dozens of workers.
 *
 * Link one of these to your library, and you're good to go.
 */

/* ### Tracing */

/* Format a logging message and write it to the log file. */
#ifdef _DEBUG
void trace(const char* function, const char* point) {
  char buffer[64];
  (void) pthread_mutex_lock(&process.trace.mutex);
  if (process.trace.count < (sizeof(buffer) / sizeof(char)) - 1) {
    sprintf(buffer, "[%s/%s]", function, point);
    /* At times, I'll insert a `fprintf(stderr, "%s\n", buffer);` right here to
     * see what's happening as it happens.
     */
    //fprintf(stderr, "%s\n", buffer);
    process.trace.points[process.trace.count++] = strdup(buffer);
  }
  (void) pthread_mutex_unlock(&process.trace.mutex);
}

static char** tracepoints() {
  return process.trace.points;
}
#else
#define trace(function, breakpoint) ((void)0)
#endif

/* ### Initialization */

/* Test the given condition and if it is true, record the given plugin attendant
 * error code and goto the given label. The label will mark an exit point for
 * the function, were cleanup will be performed. */
#define FAIL(cond, message, label) \
  do { \
    if (cond) { \
      set_error(message); \
      goto label; \
    } \
  } while (0)

/* Does nothing. Launched at initialization using the reaper thread handle, so
 * that the initial launcher has a reaper thread to join. */
static void* kickoff(void *data) {
  return NULL;
}

/* Close a pipe if it is not already closed. */
static void close_pipe(int pipeno, int direction) {
  if (process.pipes[pipeno][direction] != -1) {
    close(process.pipes[pipeno][direction]);
    process.pipes[pipeno][direction] = -1;
  }
}

/* Record the given plugin attendant error code along with the current system
 * error number. */
static void set_error(int error) {
  if (process.errors.attendant == 0) {
    process.errors.attendant = error;
    process.errors.system = errno;
  }
}

/* `initalize` &mdash; Called as the dynamic library is loaded. Must be called
 * before the plugin server process can be started.
 */

/* &#9824; */
static int initalize(struct attendant__initializer *initializer)
{
  struct sigaction sigchld;
  pthread_condattr_t attr;
  int i, pipeno, err;

  /* Otherwise, what's the point? */
  FAIL(initializer->starter == NULL, INITIALIZE_STARTER_REQUIRED, fail);
  process.starter = initializer->starter;

  FAIL(initializer->connector == NULL, INITIALIZE_CONNECTOR_REQUIRED, fail);
  process.connector = initializer->connector;

  /* The user gets to chose the canary file descriptor on UNIX. */
  process.canary = initializer->canary;

  /* Take note of the location of the relay program. */
  process.relay = strdup(initializer->relay);

  /* Initialize the pipes to -1, so we know that they are not open. */
  for (i = PIPE_STDIN; i <= PIPE_REAPER; i++) {
    process.pipes[i][0] = process.pipes[i][1] = -1;
  }

  /* Create our mutex and signaling device. */
  (void) pthread_mutex_init(&process.mutex, NULL);

  /* Initialize thread conditions. These are used to by threads to wait for
   * another thread to change the state of static variables.
   *
   * For Linux, we set a condition attribute `CLOCK_MONOTONIC`, which causes the
   * condition to use a monotonic clock when it times the timeout for a wait.
   *
   * A monotonic clock is one that always increases at a steady rate,
   * independent of the system clock, so that if someone sets the the system
   * clock back an hour while we're waiting on a 250 millisecond timeout, we
   * don't end up waiting for that extra hour.
   *
   * To get system clock safe timeouts on Darwin, we use a different strategy.
   * See `pthread_cond_waitforabit`.
   */

  /* Initialize thread conditions. */
  (void) pthread_condattr_init(&attr);

#if !defined (_POSIX_CLOCK_SELECTION)
# define _POSIX_CLOCK_SELECTION (-1)
#endif 
#if (_POSIX_CLOCK_SELECTION >= 0)
  (void) pthread_condattr_setclock (&attr, CLOCK_MONOTONIC);
#endif

  (void) pthread_cond_init(&process.cond.running, &attr);
  (void) pthread_cond_init(&process.cond.shutdown, &attr);
  (void) pthread_cond_init(&process.cond.getgpid, &attr);

  (void) pthread_condattr_destroy (&attr);

  /* Initialize the thread local storage key used to track the instance count
   * when a plugin stub threads invokes `retry`. */
  (void) pthread_key_create(&process.key, free);

  /* If the state of SIGCHLD is SIG_IGN the host application wants the kernel to
   * take care of zombies, and waitpid is usless for our purposes.
   *
   * We need a waitpid that will block until the child process exits. When
   * SIG_IGN is in play, waitpid blocks until all children exit, and then it
   * returns an ECHILD error. We're not monitoring our server process if that is
   * the case.
   *
   * It is far too presumptuous for a plugin to fix this by setting a signal
   * handler for SIGCHLD. That is, an application that was counting on the
   * kernel to reap children would probably not notice if we installed a
   * canonical SIGCHLD handler that reaped children, but its not our place. Of
   * course, setting SIGCHLD to SIG_DFT would certianly make zombies of any
   * children created by the host application that expects them to be reaped.
   *
   * We take not of the state here. If SIGCHLD is ignored, we won't call
   * `waitpid` at exit, but intead poll using the `pid`.
   */

  /* Determine if the SIGCHILD is ignored. */
  sigaction(SIGCHLD, NULL, &sigchld);
  process.waitable = sigchld.sa_handler != SIG_IGN;

  /* The plugin stub will wait for ready, so instance will be one or more before
   * retry is called. */
  process.instance = 0;

  /* Nope. Only after the initial call to start. */
  process.running = 0;

  /* Ensure that the launcher thread has a reaper thread to join. */
  pthread_create(&process.reaper, NULL, kickoff, NULL);

  /* We will preserve the same file descriptors on the plugin stub end of the
   * stdio pipes between restarts. The launcher thread is going to expect a
   * previous set, so we create it here to get the ball rolling. */
  for (pipeno = PIPE_STDIN; pipeno <= PIPE_STDERR; pipeno++) {
    err = pipe(process.pipes[pipeno]);
    FAIL(err == -1, INITIALIZE_CANNOT_CREATE_STDIN_PIPE + pipeno, fail);
  }

  /* We can close the file descriptors of the plugin server side now. We're not
   * going to actually use these particular pipes. */
  close_pipe(PIPE_STDIN, 0);
  close_pipe(PIPE_STDOUT, 1);
  close_pipe(PIPE_STDERR, 1);

  /* Create the instance pipe, which will remain open until the plugin attendant
   * is destroyed. */
  err = pipe(process.pipes[PIPE_REAPER]);
  FAIL(err == -1, INITIALIZE_CANNOT_CREATE_REAPER_PIPE, fail);

  /* No child process should inherit the instance pipe. */
  fcntl(process.pipes[PIPE_REAPER][0], F_SETFD, FD_CLOEXEC);
  fcntl(process.pipes[PIPE_REAPER][1], F_SETFD, FD_CLOEXEC);

  /* Initialize tracing mutex. */
#ifdef _DEBUG
  (void) pthread_mutex_init(&process.trace.mutex, NULL);
#endif

  trace("initialize", "success");

  /* TODO: What is success? */
  return 0;

fail:
  /* If we fail, then we close any pipes we might have opened. */
  close_pipe(PIPE_STDIN, 0);
  close_pipe(PIPE_STDIN, 1);

  close_pipe(PIPE_STDOUT, 0);
  close_pipe(PIPE_STDOUT, 1);

  close_pipe(PIPE_STDERR, 0);
  close_pipe(PIPE_STDERR, 1);

  close_pipe(PIPE_REAPER, 0);
  close_pipe(PIPE_REAPER, 1);

  return -1;
/* &mdash; */
}

/* If someone or something, say NTP, resets the system clock while we are
 * waiting on a condition, the outcome could be an aburdly long wait, hanging
 * the application. 
 *
 * With pthreads, Linux has an option to use CLOCK_MONOTONIC with the pthread
 * condition. A monotonic clock is  a clock that always advances at a steady
 * rate, even if the system clock is reset.
 *
 * Both Darwin and Linux have a monotonic clock, but Darwin does not allow you
 * to use it with pthreads, only Linux. We use the system clock.
 *
 * Darwin has a non-portable function that will do a relative wait. We implement
 * a function here that is a wrapper around that function, and an analogous
 * implementation in Linux. The pthread conditions have their clock set to the
 * CLOCK_MONOTONIC clock when they are created above.
 *
 * TK Move these notes on suprious wakeup.
 *
 * Returns the number of milliseconds remaining for the timeout. The condition
 * may be signaled before the timeout, due to [suprious
 * wakeup](https://groups.google.com/group/comp.programming.threads/msg/bb8299804652fdd7),
 * but the conditions that caused the caller to wait may not have changed. The
 * caller may want to wait for the remaining amount of time, instead of trying
 * for the the full amount.
 *
 * We wouldn't worry about this if a timeout meant that the user would recheck
 * invariants, as is the case for the reaper thread call of this function, but
 * for the call to this function from done, the timeout signifies the amount of
 * time that the 
 */
void pthread_cond_waitforabit(pthread_cond_t *cond, pthread_mutex_t *mutex,
    int millis) {
  struct timespec timespec;
  if (millis > 0) {
#ifdef __MACH__
    timespec.tv_sec = millis / 1000;
    timespec.tv_nsec = (millis % 1000) * 1000000;
    pthread_cond_timedwait_relative_np(cond, mutex, &timespec);
#else
    clock_gettime(CLOCK_MONOTONIC, timespec);
    timespec.tv_nsec += millis  * 1000000;
    timespec.tv_sec += timespec.tv_nsec / 1000000000;
    timespec.tv_nsec += timespec.tv_nsec % 1000000000;
    pthread_cond_timedwait(cond, mutex, &timespec);
#endif
  } else {
    pthread_cond_wait(cond, mutex);
  }
}

/* ### Start */

/* The start function calls the launch function. */ 
static void* launch(void *data);
/* The launch function calls the reap function. */ 
static void* reap(void *data);
/* Called by start, launch and reap. */
static void signal_termination();

/* Free the copy we made of the plugin server program name and arguments to pass
 * to the to the plugin server process.
 *
 * The second argument to our relay program is the status pipe file descriptor
 * number. We don't know that number when we make our copy of the client
 * supplied arguments, so we leave it null. We always have at least four
 * arguments in the array, so we can safely skip the second argument when it is
 * null, because it can never be the array terminator.
 */
static void free_argv() {
  int i;
  if (process.argv) {
    for (i = 0; process.argv[i] || i == 1; i++) {
      free(process.argv[i]);
      process.argv[i] = NULL;
    }
  }
}

/* Close all open pipes, except for the instance pipe and the plugin stub side
 * of the stdio pipes. The reaper pipe is never shared with children and lives
 * for the life of the plugin attendant. We keep the same file descriptor for
 * the plugin stub side of stdio between restarts, so that the plugin stub can
 * cache the file descriptors.
 */
static void close_pipes() {
  int pipeno;
  close_pipe(PIPE_STDIN, 0);
  close_pipe(PIPE_STDOUT, 1);
  close_pipe(PIPE_STDERR, 1);
  for (pipeno = PIPE_FORK; pipeno <= PIPE_CANARY; pipeno++) {
    close_pipe(pipeno, 0);
    close_pipe(pipeno, 1);
  }
}

/* The `start` function is called first at library load, then subsequently from
 * the client provided abnormal exit handler. The client can choose to launch
 * the plugin server process with different arguments each time.
 *
 *
 * You can only call this once at library load from outside of the abend
 * handler. After calling this once from outside, the function must only be
 * called from within the abend handler in the thread that invokes the abend
 * handler. You can assign new arguments and abend handlers at each restart.
 *
 * We could assert that with thread local storage for the reaper thread, and
 * passing instance numbers through the launcher thread, but we won't.
 */

/* &mdash; */
static int start(const char* path, char const* argv[])
{
  int err, argc, i, running;
  size_t size;

  /* Assert that we're not being called while the one and only plugin server
   * process is already running. You're not calling the addendant functions in
   * the correct order.
   */
  pthread_mutex_lock(&process.mutex);
  running = process.running;
  pthread_mutex_unlock(&process.mutex);

  FAIL(running, START_ALREADY_RUNNING, fail);

  /* Reset our error codes and increment the instance count. */
  pthread_mutex_lock(&process.mutex);
  process.errors.attendant = 0;
  process.errors.system = 0;
  process.instance++;
  pthread_mutex_unlock(&process.mutex);

  /* Close any pipes that might still be open. */
  close_pipes();

  /* Count the number of arguments to the plugin server program. */
  for (argc = 0; argv[argc]; argc++);

  /* Create an array to store the arguments passed to the relay program. */
  size = sizeof(char *) * (argc + 5);
  process.argv = malloc(size);
  FAIL(!process.argv, START_CANNOT_MALLOC, fail);
  memset(process.argv, 0, size);

  /* The arguments to the relay program are name of the plugin process program
   * and the plugin process program arguments. We copy those values into the
   * null terminated arguments array. The first argument is the file descriptor
   * number for the status pipe, which we've not yet created, so leave that
   * `NULL`. */
  process.argv[0] = strdup(process.relay);
  process.argv[1] = NULL;
  process.argv[2] = malloc(32);
  FAIL(process.argv[2] == NULL, START_CANNOT_MALLOC, fail);
  sprintf(process.argv[2], "%d", process.canary);

  process.argv[3] = strdup(path);
  for (i = 0; i < argc; i++) {
    process.argv[i + 4] = strdup(argv[i]);
    FAIL(process.argv[i + 4] == NULL, START_CANNOT_MALLOC, fail);
  }
  process.argv[argc + 4] = NULL;

  /* Create a launcher thread. */
  err = pthread_create(&process.launcher, NULL, launch, NULL);
  FAIL(err != 0, START_CANNOT_SPAWN_THREAD, fail);

  trace("start", "success");

  return 0;
  /* TODO Do we signal_termination here? Yes. Sort this out. */
fail:
  free_argv();
  return -1;
}

/* ### Launch */

/* Recycle the file descriptors on the plugin stub end of the stdio pipes. We
 * create a pipe in a temporary variable. Duplicate the plugin stub end of the
 * pipe, assigning it the file descriptor of the previous plugin stub file
 * descriptor. We then close the temporary plugin stub end file descriptor and
 * record the plugin server process end in our static process structure. */
static int recycle(int pipeno, int parent) {
  int temp[2], child = parent ^ 1, err;
  err = pipe(temp);
  FAIL(err == -1, LAUNCH_CANNOT_CREATE_STDIN_PIPE + pipeno, fail);
  HANDLE_EINTR(dup2(temp[parent], process.pipes[pipeno][parent]), err);
  HANDLE_EINTR(close(temp[parent]), err);
  process.pipes[pipeno][child] = temp[child];
fail:
  return err; 
}

/* Duplicate the file descriptor on plugin process server end of a stdio pipe.
 * This function is called once for each stdio pipe.  It is called after fork
 * and prior to the exec of the relay program. Only can only make async-safe
 * system calls. Errors are reported through the status pipe. */
static void duplicate(int spipe, int pipeno, int end, int fd) {
  int err; 
  HANDLE_EINTR(dup2(process.pipes[pipeno][end], fd), err);
  if (err == -1) {
    send_error(spipe, START_CANNOT_DUP_STDIN_PIPE + pipeno);
  }
}

/* It is possible for the read above to be interrupted by a signal. It does not
 * seem possible for an interrupt to occur in the middle of an eight byte read
 * such that the read is partial.  The write operation on a pipe is atomic for
 * less than PIPE_BUF which is at least 512 bytes. It would seem that the read
 * is also atomic. It would seem that it would be atomic for the 4k page size
 * that appears everywhere in Linux. It doesn't say so, and in fact, the Linux
 * man pages state that, according to POSIX, read is allowed to return a number
 * of bytes read when EINTR can occurs.
 *
 * These errors are given special error codes in the 900 range, indicating that
 * they are theoretical errors. I do not expect them to occur. If you get one of
 * these, let me know your platform.
 */

/* Encapsulates a test for an error condition that will never happen. */
#define PARTIAL_READ(actual, expected, code, label) \
  FAIL(actual != expected, PARTIAL_ ## code, label)
  
/* `launch` &mdash; Launch will fork and exec our relay program. The relay
 * program will close open file handles, reset signal handlers to the default
 * disposition, then launch the plugin server process.
 */

/* &#9824; */
static void *launch(void *data)
{
  int status, confirm, spipe, err, i, code[2];

  /* Join the reaper thread. We do not need the result. */
  pthread_join(process.reaper, NULL);

  /* Create new pipes for stdio that reuse the file descriptor of the plugin
   * stub side of the previous stdio pipes. The pipe file descriptors stay the
   * same for the life cycle of the plugin, saving us some thread
   * sychnornization headaches. */
  recycle(PIPE_STDIN, 1);
  recycle(PIPE_STDOUT, 0);
  recycle(PIPE_STDERR, 0);

  /* Create the remaning four pipes. The details of the pipes can be found in
   * the annotations above under the heading **Pipes**.
   */
  for (i = PIPE_FORK; i <= PIPE_CANARY; i++) {
    err = pipe(process.pipes[i]);
    FAIL(err == -1, LAUNCH_CANNOT_CREATE_STDIN_PIPE + i, fail);
  }

  /* Except for STDIN, all the read ends of the pipes are close on exit. */
  for (i = PIPE_STDOUT; i <= PIPE_CANARY; i++) {
    fcntl(process.pipes[i][0], F_SETFD, FD_CLOEXEC);
  }

  /* The write ends of the STDIN and FORK pipes are close on exit. */
  fcntl(process.pipes[PIPE_STDIN][1], F_SETFD, FD_CLOEXEC);
  fcntl(process.pipes[PIPE_FORK][1], F_SETFD, FD_CLOEXEC);

  /* Make the first argument to relay the string value of the status pipe. */
  spipe = process.pipes[PIPE_RELAY][1];
  process.argv[1] = malloc(32);
  FAIL(process.argv[1] == NULL, LAUNCH_CANNOT_MALLOC, fail);
  sprintf(process.argv[1], "%d", spipe);

  trace("launch", "fork");

  /* Let us fork.*/
  process.pid = fork();

  /* A zero pid means that we are the child process. */
  if (process.pid == 0) {
    /* We are in a race to fork. There is little we can do here because our host
     * application might be a multi-threaded application. Many system calls and
     * standard library calls are off limits. We can't malloc, for example. We
     * want to setup our pipes and launch our relay program, which will do the
     * reset of the cleanup. */

    /* Create a pipe for stdout.  */
    duplicate(spipe, PIPE_STDIN, 0, STDIN_FILENO);
    duplicate(spipe, PIPE_STDOUT, 1, STDOUT_FILENO);
    //duplicate(spipe, PIPE_STDERR, 0, 1, STDERR_FILENO);

    /* Duplicate the pulse pipe to the file descriptor specified at attendant
     * initialization.  dup2 will close the target file descriptor if it is
     * open. If the aribitrarily chosen fd assigned to the write end of the pipe
     * is by conicidence the canary file descriptor, dup2 does nothing. */
    HANDLE_EINTR(dup2(process.pipes[PIPE_CANARY][1], process.canary), err);

    execv(process.relay, process.argv);

    /* If we are here, we did not execv. If we execv, the program is replace
     * with the relay program so this code is not executed. If we are here, we
     * report the error that occurred at execv. */ 
    send_error(spipe, START_CANNOT_EXECV);

    /* We must call _exit and not exit. The exit call will trigger any atexit
     * handlers registered by the host application. The _exit call will not. */

    /* Failure. */
    _exit(EXIT_FAILURE);
    /* */
  }

  /* We are the parent. The child will never get here because of either the
   * execve or the _exit. */

  /* We have failed to do so much as fork. Why does fork fail? Not enough
   * memory to copy the process, not enough memory in the kernel to allocate the
   * housekeeping, or there is resource limit on the number of processes. */
  FAIL(process.pid == -1, LAUNCH_CANNOT_FORK, fail);

  /* Close the child end of all of the pipes we've just created. We do not close
   * the reaper pipe, of course, because it lasts for the life time of the
   * plugin attendant. */
  close_pipe(PIPE_STDIN, 0);
  close_pipe(PIPE_STDOUT, 1);
  close_pipe(PIPE_STDERR, 1);
  close_pipe(PIPE_FORK, 1);
  close_pipe(PIPE_RELAY, 1);
  close_pipe(PIPE_CANARY, 1);

  /* Wait for the fork pipe to close. It will be a read that returns zero bytes.
   * We're only interested in a successful return. The buffer will be empty. */
  HANDLE_EINTR(read(process.pipes[PIPE_FORK][0], &confirm, sizeof(confirm)), err);
  
  /* Close the file descriptor of the plugin stub end of the status pipe. */
  close_pipe(PIPE_FORK, 0);

  /* Many of the error checks below are assertions, not exception handlers.
   *
   * What follows are a lot of error conditions that can only occur if someone,
   * probably me, breaks the plugin attendant itself. We're testing that the
   * operating system does indeed pass to a program the arguments we ask it to
   * pass. We're testing that when we write to a pipe, that the same data will
   * be read from the pipe.
   *  
   * We're confirming the values that we've fed to the relay program as
   * arguments, that the relay program is echoing back to us through its pipes.
   * This is were checking every error code becomes academic.
   *
   *
   * This is why test coverage, especially at this level, can be a rabbit hole.
   * To get 100% coverage, we'd have to create a different version of the relay
   * program that triggered errors that are theoretical. It would have to send
   * only three bytes of an integer, to simulate a partial pipe read. A pipe
   * write of under 512 bytes is atomic, so the only way to trigger this error
   * is to create a bogus relay program, or else hack the operating system.
   */

  /* The relay will write the status pipe file descriptor to stdandard out. */
  HANDLE_EINTR(read(process.pipes[PIPE_STDOUT][0], &confirm, sizeof(confirm)), err);

  /* If zero, the standard I/O pipe hung up, it means the relay did not execute
   * or exited immediately. */
  if (err == 0) {
    /* Read the error code. */
    HANDLE_EINTR(read(process.pipes[PIPE_RELAY][0], code, sizeof(code)), err);

    /* Zero bytes returned means the relay program exited immediately because we
     * fed it a malformed status pipe file descriptor.  It won't write an error
     * because it doesn't have a status pipe. You can see above that we format
     * the file descriptor correctly, so consider this an assertion. */
    FAIL(err == 0, LAUNCH_IMMEDIATE_RELAY_EXIT, fail); 
  
    /* Theoretical error. Read is probably atomic for under 4k. */
    PARTIAL_READ(err, sizeof(code), FORK_ERROR_CODE, fail);

    /* Set the plugin attendant error code and system error number. */
    process.errors.attendant = code[0];
    process.errors.system = code[1];

    /* Abend. */
    goto fail;
  /* &mdash; */
  } else {
    /* Theoretical error. Read is probably atomic for under 4k. */
    PARTIAL_READ(err, sizeof(confirm), STDOUT_STATUS_PIPE_NUMBER, fail);

    /* Assert that we passed the correct file descriptor through stdout. */
    FAIL(confirm != spipe, LAUNCH_RELAY_PIPE_STDOUT_FAILED, fail); 

    /* Read the status pipe file descriptor number from the status pipe itself. */
    HANDLE_EINTR(read(process.pipes[PIPE_RELAY][0], &confirm, sizeof(confirm)), err);

    /* Assert that the relay pipe is still open. */
    FAIL(err == 0, LAUNCH_RELAY_PIPE_HUNG_UP, fail); 

    /* Theoretical error. Read is probably atomic for under 4k. */
    PARTIAL_READ(err, sizeof(confirm), STATUS_PIPE_NUMBER, fail);

    /* Assert that we passed the correct file descriptor through the status pipe. */
    FAIL(confirm != spipe, LAUNCH_RELAY_PIPE_STDOUT_FAILED, fail); 
  /* */
  }

  /* Now know that our status pipe is setup correctly, read an error if any. */
  HANDLE_EINTR(read(process.pipes[PIPE_RELAY][0], code, sizeof(code)), err);

  /* Zero means we hung, up. which is wonderful. Our plugin server process is up
   * and running. Otherwise, the relay program encoutered an error. */
  if (err != 0) {
    /* Theoretical error. Read is probably atomic for under 4k. */
    PARTIAL_READ(err, sizeof(code), EXEC_ERROR_CODE, fail);

    /* Set the plugin attendant error code and system error number. */
    process.errors.attendant = code[0];
    process.errors.system = code[1];

    /* Abend. */
    goto fail;
  }

  /* Call the application developer provided connector to initiate the plugin
   * stub to plugin server process IPC. */
  process.connector(process.pipes[PIPE_STDIN][1], process.pipes[PIPE_STDOUT][0]);

  /* Our server process is now up and running correctly. Time to launch the
   * reaper thread. This reaper thread monitor the plugin server process for
   * termination. */
  pthread_create(&process.reaper, NULL, reap, NULL);

  /* Don't need these anymore. */
  free_argv();

  trace("launch", "success");

  /* */
  return NULL;

fail:

  trace("launch", "failure");

  if (process.pid > 0) {
    /* There is no logic in the relay that doesn't exit immediately. If it is
     * hung and a `SIGKILL` is necessary, then plugin attendant is broken. */
    kill(process.pid, SIGKILL);

    /* Reap the process. If the host application is set to ignore `SIGCHLD` then
     * we skip this step. */
    if (process.waitable) {
      HANDLE_EINTR(waitpid(process.pid, &status, 0), err);
    }
  }

  /* Close the pipes we created. */
  close_pipes();

  /* Release the arguments. */
  free_argv();

  /* We go into our abend procedure. */
  signal_termination();

  return NULL;
}

/* ### Reaper */

/* The reaper thread waits for the plugin server process to exit by polling to
 * the canary pipe for hang up. We do this because we cannot count on `waitpid`.
 * 
 * There are two ways in which the exit status from the server process may be
 * intercepted. First, we might not be able to use `waitpid` at all, because the
 * host program set `SIGCHLD` to `SIG_IGN`. Second, if `SIGCHLD` is waitable,
 * the host application might be doing a blanket wait, and it might reap the
 * child process, obtaining the pid, leaving us with an `ECHILD` error. Thus, we
 * don't bother saving the exit code.
 */

/* &#9824; */
static void* reap(void *data)
{
  static int REAPER = 0, CANARY = 1;
  int input[2], instance = 0,
    sig = SIGTERM, timeout = -1, hangup = 0, shutdown = 0;
  int status, err, fds[2], i, j, count;
  struct pollfd channels[4];
  char buffer[2048];

  fds[0] = process.pipes[PIPE_STDOUT][0];
  fds[1] = process.pipes[PIPE_STDERR][0];

  /* Join the reaper launcher. We do not need the result. */
  pthread_join(process.launcher, NULL);

  /* Tell the library stub functions that we are running. */
  (void) pthread_mutex_lock(&process.mutex);
  process.running = 1;
  process.restarting = 0;
  (void) pthread_cond_signal(&process.cond.running);
  (void) pthread_mutex_unlock(&process.mutex);

  /* Loop until the plugin server process exits. */
  do {
    /* The other end of the canary pipe is held by the library server process.
     * It is not used for communication, only to detect the termination of the
     * library server process. When it closes, we know we are terminated.
     * 
     * The instance pipe is a way for the plugin stub to tell the reaper thread
     * that the library server process has become unresponsive. We can be awoken
     * by the instance pipe, telling us that the library stub functions have
     * detected a dead server process. When we get an instance number, we will
     * kill the running process if the instance number is greater than the
     * current process instance number.
     * 
     * If instance number is -1, a shutdown is pending and we continue to wait
     * on the canary pipe. When it closes, we know not to restart the server. */

    /* Poll the instance and canary pipes. */
    channels[REAPER].events = POLLIN;
    channels[REAPER].revents = 0;

    /* Might be visiting this twice, due to SIGTERM, then getting a new instance
     * number. Ah, but the instance number can't be greater than the current
     * number, because it only gets incremented in the launcher thread, so this
     * is okay. Unless we get a scram, but that is fine too.
     */

    /* */
    channels[CANARY].events = POLLHUP;
    channels[CANARY].revents = 0;

    channels[REAPER].fd = process.pipes[PIPE_REAPER][0];
    channels[CANARY].fd = process.pipes[PIPE_CANARY][0];

    /* We're going to simply drain standard out and standard error of the plugin
     * server process. We do not log the output. It would be just as reasonable
     * to close the pipes, or ignore them, but we drain them as long as we have
     * this loop to drain them with. */

    /* */
    count = 2;
    for (i = 0; i < sizeof(fds) / sizeof(int); i++) {
      if (fds[i] != -1) {
        channels[count].events = POLLIN | POLLHUP;
        channels[count].revents = 0;
        channels[count].fd = fds[i];
        count++;
      }
    }

    trace("reap", "poll");

    HANDLE_EINTR(poll(channels, count, timeout), err);

    /* Not terribly concerned about errors here. If we encounter them, we ignore
     * the pipes, so problems flow back to the server process.
     *
     * TODO Test me. Write some junk to standard out.
     */
    for (i = 2; i < count; i++) {
      if (channels[i].revents & POLLIN) {
        HANDLE_EINTR(read(channels[i].fd, buffer, sizeof(buffer)), err);
        if (err == -1) {
          channels[i].revents = POLLHUP;
        }
      }
      if (channels[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
        for (j = 0; j < sizeof(fds) / sizeof(int); j++) {
          if (fds[j] == channels[i].fd) {
            fds[j] = -1;
          }
        }
      }
    }

    /* Note that, errors here make the situation hopeless. If we encouter errors
     * with the process monitoring pipes, we go to the shutdown state. */

    /* Did the monitored process terminate? */
    if (channels[CANARY].revents & POLLHUP) {
      trace("reap", "hangup");
      hangup = 1;
    } else if (channels[CANARY].revents != 0) {
      set_error(REAPER_UNEXPECTED_CANARY_PIPE_EVENT);
    }

    /* Did we get an instance number from the plugin stub? */
    if (channels[REAPER].revents & POLLIN) {
      /* Read the message from the user. */
      HANDLE_EINTR(read(process.pipes[PIPE_REAPER][0], input, sizeof(input)), err);

      if (err != sizeof(input)) {
        /* Any error reading the instance pipe means we shutdown for good. */
        if (err == -1) {
          set_error(REAPER_CANNOT_READ_REAPER_PIPE);
        } else {
          set_error(REAPER_TRUNCATED_READ_REAPER_PIPE);
        }

      } else if (input[0] == -1) {
        trace("reap", "shutdown");
        shutdown = 1;
      } else if (input[0] > instance) {
        /* We will restart if we get an instance number higher than the static
         * instance number. If we get a `-1` we shutdown. */
        trace("reap", "instance");
        instance = input[0];
        /* */
      }
    } else if (channels[REAPER].revents != 0) {
      set_error(REAPER_UNEXPECTED_REAPER_PIPE_EVENT);
    }

    /* If we're getting unexpected errors from reading the pipes, we've entered
     * an unstable state. We don't want the plugin attendant itself to hang, and
     * it can't seem to rely on useful behavior from pipes, so we nuke it from
     * orbit. It's the only way to be sure. */
    if (process.errors.attendant) {
      /* Trigger a shutdown. */
      shutdown = 1;
      /* Leave the reaper pipe polling loop. */
      hangup = 1;
      /* Nuke it from orbit. It's the only way to be sure. */
      kill(process.pid, SIGKILL);
    }

    /* Note if we got a shutdown from the instance pipe. The next time we detect
     * that the plugin server process has exited, we will not try to restart it.
     *
     * We trigger both shutdown and running thread conditions. The shutdown
     * function waits to know that the process has shutdown. Other functions
     * wait for the process to run again, and this will wake them so then can
     * see that the process will never run again. 
     */
    if (shutdown) {
      (void) pthread_mutex_lock(&process.mutex);
      process.shutdown = 1;
      (void) pthread_cond_signal(&process.cond.running);
      (void) pthread_cond_signal(&process.cond.shutdown);
      (void) pthread_mutex_unlock(&process.mutex);
      shutdown = 0;
    }

    /* If we are restarting but have not received a hang up, kill. First with a
     * `SIGTERM` then with a `SIGKILL`. We give the process a half second to
     * shutdown after a `SIGTERM`, but wait indefinately after the `SIGKILL`. */
    if (instance > 0 && !hangup) {
      kill(process.pid, sig);
      timeout = sig == SIGTERM ? input[1] : -1;
      sig = SIGKILL;
      continue;
    }
  /* Repeat until the plugin server process exits. */
  } while (!hangup);

  trace("reap", "hungup");

  /* TODO Log restart reason? No. We have no good reason. Or, hmm... Sure, why
   * not? */

  /* The waitable flag was determined at initialization. We never check again.
   * A host application that changes `SIGCHLD` disposition every now and again
   * is far to shabby to support. */

  /* If we are waitable use `waitpid`. */
  if (process.waitable) {
    /* Our host application might also be waiting on the pid, by using a global
     * wait to wait until any child terminates. That means that the host
     * application might be the one to reap the child. If that is the case, then
     * we got an error and errno is ECHILD, meaning that the child does not
     * exist. The non-existance of the child is death enough for our purposes.
     *
     * So, ECHILD is not really an error, we are retrying on EINTR, so that
     * leaves EINVAL, which we're not going to trigger. Let's move on.  */

    /* We wait for the child process to exit, blocking until it exits. */
    HANDLE_EINTR(waitpid(process.pid, &status, 0), err);
  /* &mdash; */
  } else {
    /* There is a theoretical race condition, where the process id may be
     * reused. When a pid is released by the operating system, the operating
     * system can reuse it. It is released when the child process is reaped,
     * when wait is called, or child signals are ignored or simply not generated
     * due do SIGCHLD beign set to SIG_IGN, as is the case here.
     * 
     * We have a race condition where the the library server child process
     * exits, the host program spawns a new child, and that new child is
     * assigned the pid of the dead, reaped library server child process. If
     * this occurs during our timed wait, then when we wake up, kill will tell
     * us that, yes, there is a child with the pid you requested, and it is
     * alive and kicking.
     *
     * On the target operating systems targeted at the time of writing, pids
     * appear to get assigned in sequential order, wrapping when the pid values
     * approach the maxiumum value of the pid type. Which means that we're in a
     * race with 60,000 process starts and exits. Likely we will win.
     *
     * Then, the pid, when it does comes around again, needs to be assigned to a
     * child process spawned by our the owner of our host application, because
     * kill can only send signals to processes owned by the same user. (Unless
     * we're running as root, but that is madness.)
     *
     * There are operating systems that, for security reasons, [assign pids
     * randomly](http://www.faqs.org/faqs/aix-faq/part2/section-20.html). This
     * may make it more likely that our pid will get reassigned to a new
     * process, baring any logic in the randomizer that holds off on reusing a
     * pid for a few ticks. Again, it has to be the same user as the host
     * application that draws the pid for it to be problem.
     *
     * It is one of those things were the standard promises only so much. The
     * standards says only that the pid is reserved until the child is reaped.
     * 
     * Thus, there is a theoretical race condition here. In fact, the same race
     * condition applies to the `waitpid` branch if the host application reaps
     * the child before us. Oh, the peril.
     */

    /* Loop while the pid is still valid. */
    while (getpgid(process.pid) != -1) {
      /* Wait for a quarter of a second and check the pid again. We use a
       * condition that is used only for this test. We want the timeout, not the
       * signaling. The wait will cause us to release the mutex. */
      pthread_mutex_lock(&process.mutex);
      pthread_cond_waitforabit(&process.cond.getgpid, &process.mutex, 250);
      pthread_mutex_unlock(&process.mutex);
      /* */
    }
  }

  /* Cleanup. */
  signal_termination();

  /* The thread return value is unused. */
  return NULL;

  /* &mdash; */
}

/* Cleanup when we fail to start the launcher thread, fail to launch the plugin
 * server program, or detect that the plugin server process has exited. */

/* */
static void signal_termination() {
  int instance;

  /* Don't need the process identifier anymore. */
  process.pid = 0;

  /* Dip into our mutex. */
  (void) pthread_mutex_lock(&process.mutex);

  /* Take note of whether we sould invoke the abend handler. Reset for an
   * orderly restart. Do not reset shutdown here, only stopped. */
  process.restarting = !process.shutdown;
  process.running = 0;
  instance = process.instance;

  /* Signal any thread waiting on a running state change. */
  (void) pthread_cond_signal(&process.cond.running);

  /* Undip. */
  (void) pthread_mutex_unlock(&process.mutex);

  /* If we've decided to try a restart, call the abend handler. */
  if (process.restarting) {
    /* Call the starter to restarter the server process. */
    process.starter(1);

    /* If the abend handler did not call start, then it has decided to shutdown.
     * We are no longer restarting, and we can signal a process state change to
     * wake any library stub threads waiting on restart in shutdown.
     *
     * Trigger both shutdown and running. The shutdown function waits to know
     * that the process has shutdown. Other functions wait for the process to
     * run again, and this will wake them so then can see that the process will
     * never run again. */
    (void) pthread_mutex_lock(&process.mutex);
    if (process.instance == instance) {
      trace("terminate", "shutdown");
      process.restarting = 0;
      process.shutdown = 1;
      (void) pthread_cond_signal(&process.cond.running);
      (void) pthread_cond_signal(&process.cond.shutdown);
    }
    (void) pthread_mutex_unlock(&process.mutex);
  }
}

/* ### Ready
 *
 * `ready` &mdash; Called by the plugin stub after the initial call to start to
 * wait for the plugin server process to start.
 */

/* &#9824; */
static int ready() {
  int ready;

  /* We block until either we are ready or have entered the shutdown state. If
   * we enter the shutdown state, we know that we will never run again. */
  pthread_mutex_lock(&process.mutex);
  while (! process.running && ! process.shutdown) {
    pthread_cond_wait(&process.cond.running, &process.mutex);
  }
  ready = ! process.shutdown;
  pthread_mutex_unlock(&process.mutex);

  trace("ready", "exit");

  return ready;
}

/* ### Retry */

/* After initialization, the plugin server process is supposed to run, without
 * interruption, until the orderly shutdown of the plugin server process, prior
 * to unloading the plugin library.
 *
 * The plugin attendant exists to launch the plugin server process, and to
 * restart it quickly in the event of an early, unexpected exit. The plugin
 * attendant will notice that the plugin server process has disappeared. It will
 * call the client supplied abend function, which can in turn tell the plugin
 * attendant to start the plugin server process, or it can descide to give up
 * and make the plugin unavailable.
 *
 * Early, unexpected termination of the plugin server process creates the
 * potential for a disruption of server perceptible to the end user. This
 * potential cannot be eliminated.
 *
 * Do not take this as an admonishment to write a plugin server process that
 * crashes never. Quite the opposite. The nice part about an out-of-process
 * architecture is that the consequences of failure are isolated. If you detect
 * that the plugin server process is in an invalid state, you can crash it and
 * get a new clean state. Leave a log behind and the new process can send a
 * crash report back to the mothership (with the end users's blessing) as its
 * first action on recovery.
 *
 * You must, however, accept that there will be a seam in your plugin that the
 * user may have an occasion to see. You must develop a tolerence for
 * imperfection, because intolerence for imperfection is how crazy is made. A
 * belief that you have eleminated imperfection is the inevitable delusion. But,
 * we're not Java programmers here.
 *
 * A strategy of crashing in response to exceptions means that you're plugin
 * server process needs to be reentrant. That is, you should be able to
 * terminate at any point, and the new process will be able to pick up the
 * peices and resume where you left off.
 *
 * This is going to be hard to understand for those of you who are steeped in
 * the Kabuki of try/catch exception handling, or worse, checked exceptions.  It
 * is a different approach from try/catch exception handling. You build your
 * exception handling into your development cycle. You do not try to build it
 * into your function call stack.
 *
 * A plugin server process that crashes when no one is looking will be restarted
 * by the plugin attendant. It will probably go unnoticed by the end user.
 *
 * It is far more likely that the plugin server process will crash while
 * servicing a request of the plugin stub. The plugin stub will probably detect
 * the crash. It will need to wait for restart to retry the request on behalf of
 * the user.
 *
 * The plugin stub is going to be using pipes, sockets or message queues to
 * communicate with the server process. Any reasonable method of IPC will have
 * ways to report that the other participant is missing, or offer a way to
 * timeout a request. The plugin stub must use these mechanisms to detect an
 * unresponsive server. It cannot take the presence of plugin process server for
 * granted.
 *
 * There is no way for the plugin attendant to erect a barrier that the plugin
 * stub could rely upon to prevent it from connecting to a dead or hung plugin
 * process server. The crash may come at any time, perhaps at the moment just
 * before a pipe read. Rely instead on the operating system to report the
 * success or failure of inter-process communication.
 *
 * The plugin stub may detect that the plugin server process has become
 * unresponsive. If the plugin server process is hung, the plugin attendant has
 * no way of knowing. In this case, the plugin stub will have to initiate an
 * early, unexpected shutdown, then wait for the restart.
 *
 * This is, in fact, our default action when the plugin stub detects that the
 * plugin process server must restart. It signals to the plugin attendant that
 * the plugin attendant should restart the plugin process server, if the plugin
 * attendant hasn't already detected the failure itself.
 *
 * The `retry` function of plugin attendant has a mechanism that will prevent a
 * race condition where the plugin attendant and one or more plugin stub threads
 * all discover that the plugin server process has crash, and all race to
 * restart it. In such a case, the plugin server process would be restarted more
 * than once for a single detected crash. This may lead to infinate restarts as
 * plugin threads would demand a restart of a crashed server, that was
 * crashed only because other plugin threads had just demanded the same.
 */

/* `retry` &mdash; Report that IPC with the plugin server process failed,
 * indicating that the plugin server process is either crashed or hung.
 *
 * The `retry` method will checks the last instance number of the plugin server
 * process known to the current thread. If the currently running plugin server
 * process has an instance number that is greater, retry will return true and
 * the plugin stub should retry communication with the plugin process server. If
 * the instance numbers are equal, and the plugin attendant believes that the
 * plugin server process is running, then the running flag is set to false, and
 * the reaper process is send the instance number through the instance pipe.
 */

/* &#9824; */
static int retry(int milliseconds) {
  int err, *instance, terminate = 0, message[2];

  /* Get the current value of the thread local instance. */
  instance = ((int*) pthread_getspecific(process.key)); 
  
  /* If there is no instance, we allocate one. The cleanup function associated
   * with the thread local key will free the pointer at thread exit. */
  if (instance == NULL) {
    instance = malloc(sizeof(int));
    *instance = 1;
    (void) pthread_setspecific(process.key, instance);
  }

  /* Dip into our mutex. */
  (void) pthread_mutex_lock(&process.mutex);

  fprintf(stderr, "Process instance is %d and %d term %d\n", process.instance, *instance, terminate);
  /* If the process instance equals our thread local instance and the process is
   * running, then we are the first stub thread to report that this instance has
   * died. */
  if (process.instance == *instance && process.running) {
    /* We are the only client thread that can reach this point. We mark the
     * plugin server process as not running. We will send a message to the
     * reaper thread to restart the plugin server thread, but outside of this
     * mutex. */
    process.running = 0;
    terminate = 1;
    /* */
  }
  fprintf(stderr, "Process instance is %d and %d term %d\n", process.instance, *instance, terminate);

  /* Undip. */
  (void) pthread_mutex_unlock(&process.mutex);

  /* Send the instance number through the pipe. This wakes the reaper thread and
   * tells it that the given instance has hung. The reaper process will kill the
   * thread using SIGTERM, then SIGKILL.
   *
   * TK Already awake.
   */
  if (terminate) {
    trace("retry", "terminate");

    message[0] = *instance;
    message[1] = milliseconds;
    HANDLE_EINTR(write(process.pipes[PIPE_REAPER][1], message, sizeof(message)), err); 
  }

  /* Wait for the server to be ready again. */
  if (ready()) {
    /* Grab the instance number state. */
    (void) pthread_mutex_lock(&process.mutex);
    *instance = process.instance;
    (void) pthread_mutex_unlock(&process.mutex);

    /* Stash the instance number in thread local storage. */
    (void) pthread_setspecific(process.key, instance);

    trace("retry", "again");

    /* Return true to indicate the IPC is running again. */
    return 1;
  }

  trace("retry", "shutdown");

  /* Return false if we've shutdown, indicating that a retry of IPC is
   * pointless. */
  return 0;
  /* */
}

/* ### Shutdown
 *
 * `shutdown` &mdash; Orderly shutdown is when we tell the plugin attendant to
 * expect an exit, and to not attempt to recover from the exit, it is the final
 * exit before the plugin is unloaded.
 *
 * The happy path is as follows.
 *
 * * The plugin stub calls shutdown to inform the plugin attendant that shutdown
 * is pending. The plugin attendant will not attempt restart the next time it
 * detects plugin server process shutdown.
 * * The plugin stub should use its chosen method of IPC to inform the
 * server process to shutdown.
 * * The plugin stub should then use the `done` function to wait for the
 * attendant to shutdown.
 *
 * This procedure should take place when the plugin library is unloaded. It must
 * be performed once and should not be called twice, or worse, at the same time
 * from multiple threads.
 *
 * We assume that when the plugin stub calls shutdown that it is not waiting on
 * completion of calls to the plugin server process, and that it will make no
 * more calls to the plugin server process after shutdown is called.
 *
 * The unhappy path is that the libary server process has crashed and is in
 * the midst of a recovery when the plugin stub calls initiates shutdown. The
 * plugin stub will use a form of IPC to request shutdown of a plugin server
 * process that is not running, that the plugin attendant is restarting.
 *
 * The plugin stub should account for this condition. If the call to initiate an
 * orderly shutdown hangs up, call the `scram` function to terminate the plugin
 * server process with extreme prejudice. Then use the `done` function to wait
 * on exit.
 *
 * We do not attempt to restart a crashed server so that we can tell it to
 * shutdown. You need to design a plugin server process that does not perform an
 * elaborate shutdown.
 */

/* */
static int shutdown() {
  int err, running, shutdown[2] = { -1, 0 };

  /* Tell the reaper thread that shutdown has come. It will not attempt to
   * restart the library server process the next time it exits. */
  HANDLE_EINTR(write(process.pipes[PIPE_REAPER][1], shutdown, sizeof(shutdown)), err);

  /* Dip into our mutex to check and see we're not in the middle of a server
   * restart. If we are in the middle of a server restart, we may as well wait
   * for it to finish before we continue. */
  (void) pthread_mutex_lock(&process.mutex);
  while (process.restarting) {
    trace("shutdown", "restarting");
    (void) pthread_cond_wait(&process.cond.running, &process.mutex);
  }

  /* Wait for the shutdown flag to set, otherwise a call to done is going to
   * report an invalid state. */
  while (! process.shutdown) {
    trace("shutdown", "shutdown");
    (void) pthread_cond_wait(&process.cond.shutdown, &process.mutex);
  }

  /* If we invoke the user abend function, and it doesn't want to restart, then
   * we will be both terminated and shutdown. We may have failed to launch the
   * process. The process may have launched and crashed, and the reaper did the
   * reaping befere this thread could make process. In any case, we are now
   * shutdown and terminated, so there is no point in trying to send a shutdown
   * signal to the never to be again library server process.
   *
   * Also, even though we are in the running state, the process might be
   * crashing at this moment, so the orderly shutdown signal might not work. We
   * cant' say that it will work, but in certain cases we can say that it won't.
   */

  /* Note if we are running. */
  running = process.running;

  (void) pthread_mutex_unlock(&process.mutex);

  trace("shutdown", "exit");

  return running;
}

/* Want a timeout to escalate to kill. Or does kill happen in here? */
static int done(int timeout) {
  int done, shutdown;

  pthread_mutex_lock(&process.mutex);
  if ((shutdown = process.shutdown)) {
    /* TODO Is this what we're waiting for? */
    /* TODO Need to fix timeouts to that they try again if they exit early. */
    if (process.running) {
      do {
        pthread_cond_waitforabit(&process.cond.running, &process.mutex, timeout);
      } while (process.running && timeout <= 0);
    }
  }
  done = ! process.running;
  pthread_mutex_unlock(&process.mutex);

  /* TODO Are you going to join a destroyed process? No, but multiple joins are
   * bad. TK Document that you can only call this from one thread. */
  if (done) {
    pthread_join(process.reaper, NULL);
  }

  trace("done", "exit");

  return done;
}

/* Shutdown immediately with a SIGKILL. */

/* TODO What is the correct return value? */

/* */
static int scram() {
  int err, scram[] = { INT_MAX, -1 };
  /* Must be able to send two shutdowns. Then poll must be able to detect that
   * not all of the stuff has been read, poll must not block if the buffer is
   * not drained. */
  if (shutdown()) {
    /* Send a huge instance number down the pipe. This is going to trigger a
     * restart, an unrecoverable one, and a last one. No other `scram` or
     * `retry` will be able to trigger a restart, because they will not be able
     * to send a greater instance number to supersede the maxium instance number
     * send by this `scram`.
     *
     * We'll try a SIGTERM term first, as usual, then a SIGKILL.
     */
    HANDLE_EINTR(write(process.pipes[PIPE_REAPER][1], scram, sizeof(scram)), err);

    trace("scram", "initiated");

    return 1;
  }

  trace("scram", "shutdown");

  return 0;
}

/* Return the last error recorded by the attendant. */

/* &#9824; */
static struct attendant__errors errors() {
  return process.errors;
}

/* Called when the library unloaded. This will not shutdown the server process.
 * You must shutdown the server process, though. Do that before calling destroy.
 */

/* &#9824; &mdash; */
static int destroy() {
#ifdef _DEBUG
  int i;
#endif

  /* Release our mutex and signaling devices. */
  pthread_mutex_destroy(&process.mutex);
  pthread_cond_destroy(&process.cond.running);
  pthread_cond_destroy(&process.cond.shutdown);
  pthread_cond_destroy(&process.cond.getgpid);

  /* Release the path to the relay program. */
  free(process.relay);
  process.relay = 0;

  /* Release the file descriptors reserved for the plugin stub side of the stdio
   * pipes. */
  close(process.pipes[PIPE_STDIN][1]);
  close(process.pipes[PIPE_STDOUT][0]);
  close(process.pipes[PIPE_STDERR][0]);

  /* Release the reaper pipe. */
  close(process.pipes[PIPE_REAPER][0]);
  close(process.pipes[PIPE_REAPER][1]);

  /* Release the trace points, if any. */
#ifdef _DEBUG
  for (i = 0; process.trace.points[i]; i++) {
    free(process.trace.points[i]);
  }

  (void) pthread_mutex_destroy(&process.trace.mutex);
#endif

  trace("scram", "success");

  /* Success. */
  return 0;

/* &mdash; */
}

struct attendant attendant =
{ initalize
, start
, ready
, retry
, shutdown
, done
, scram
, errors
, destroy
#ifdef _DEBUG
, tracepoints
#endif
};

/* Had a realization while considering restart. I'd initially thought that I'd
 * leave it to the client programmer to choose between polling or an abend
 * callback, but in the end I realized that I don't have to support a use case
 * that I'm not going to use. This is me imaginging someone chastising me for
 * not offering an option, an arbitrary option, that occured to me out of
 * nowhere. To my mind, someone will request this someday, so I'd better think
 * about it now.
 *
 * Who? When? And why must I now design this for them? Why must I test it?  Why
 * is it on me to support the mythical guy shouting gimmie in my inbox someday?
 * They ought to provide the patch, the tests, and the justification for adding
 * a logical paths that do not yet exist.
 */
