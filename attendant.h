/* A process monitor that watches a single out-of-process server on behalf of a
 * dynamically loaded plugin running within the context of a host application. 
 *
 * ## On Documentation
 *
 * I'm documenting what I've written. I'm not defending what I've written. If
 * you think you can do better, submit a patch. If I disagree, then fork. If you
 * find a bug, tell me, please. Otherwise, before you tell me that there is
 * something wrong, try to make sure you understand the inherient compromises.
 *
 * It took a longer to write this than I would have expected. If you don't have
 * time to read the documentation, I can't read it to you. If you were to email
 * me directly, I might type out something just about as long. But, please, do
 * ask questions, and suggest documentation patches if this is confusing.
 *
 * &mdash;
 *
 * *Now Helicon must needs pour forth for me, And with her choir Urania must
 *  assist me, To put in verse things difficult to think.* &mdash; Dante
 *
 * ## Origins
 *
 * The plugin attendant was written for use in an
 * [NPAPI](https://wiki.mozilla.org/NPAPI) plugin, but there is no NPAPI
 * specific code here. There are no NPAPI function calls or header files.
 *
 * I imagine that this plugin attendant could work with any similar plugin
 * architecture, where a host application loads a plugin that is implemented as
 * dynamically linked library.
 *
 * If you've found this, you've probably discovered that it is no simple matter
 * to launch a process from an NPAPI plugin, let alone monitor it, signal it,
 * and obtain its exit status. It really is not that simple.
 *
 * ## Purpose
 *
 * The plugin attendant launches and monitors a single process.
 *
 * With this single process, you can implement an out-of-process plugin
 * architecture. Your plugin functions as a proxy to an out-of-process plugin
 * server that performs the real work of the plugin. It isolates the complexity
 * of the plugin in a separate process, so that a catastrophic error will
 * destabilize only the plugin server, and not destabilize the host application.
 *
 * The out-of-process architecture divides a plugin into a plugin stub, a plugin
 * attendant, and a plugin server process. The plugin stub is the library loaded
 * by the host application that implements the plugin interface according to the
 * plugin API. The plugin server process is an out-of-process service that
 * implements the plugin logic. The plugin attendent, seen here, monitors the
 * process, restarting it if it exists unexpectedly.
 *
 * Within its out-of-process container, your plugin is free to do whatever it
 * wants, to take whatever shape it needs to take. It is not limited by the
 * architecture of the host application. It has its own process space. It
 * doesn't have to worry about deadlock against and races with unknown host
 * application threads. It can use the full compilment of services available to
 * it on its host operating system.
 *
 * ## Usage
 *
 * The plugin attendant launches plugin server process when the plugin library
 * loads. It monitors the plugin server process while the plugin is in service.
 * It terminates the plugin server process when the plugin unloads.
 *
 * Although technically a child process on UNIX, the plugin server process does
 * not act as a child in the classic pre-fork worker architecture. The plugin
 * server acts as a daemon, an isolated and entirely separate process running on
 * the same machine as the plugin stub. It may as well have been launched by the
 * operating system as a system service. 
 *
 * The plugin attendant does not provide a framework for an out-of-process
 * plugin architecture. It does provide the essential service of launching and
 * monitoring the external server from within the host application. It does not
 * provide a inter-process communication strategy. You can use the IPC
 * facilities of your operating system to design an ideal strategy for your
 * plugin.
 *
 * The plugin attendant maintains a set of redirected standard I/O pipes,
 * standard input, standard output, and standard error. These pipes serve as the
 * minimum inter-process communication channel between the plugin stub and the
 * plugin server process. You can use standard I/O and command line arguments to
 * estabish furher inter-process communication channels, using other operating
 * system facilities such as message queues, named pipes and TCP/IP sockets.
 *
 * You might decide that standard I/O is sufficient for inter-process
 * communication between your plugin stub and the plugin server process. You are
 * still responsible for desiging a protocol that will use the standard I/O
 * pipes.
 *
 * ## Plugin Server Process Requirements
 *
 * The plugin server process will be like so...
 *
 * * There is one and only one instance of the plugin server process to monitor.
 * * You'll know exactly where the program file is on the file system before you
 * try to start it. No search paths here.
 * * The plugin server process runs as long as the library is loaded and it
 * shuts down when the library is unloaded.
 * * On UNIX, the plugin server process will inherit an open file descriptor,
 * with a file descriptor number of your choice. It is a canary file descriptor
 * used to detect plugin server process exit. The plugin server process will not
 * read from, write to, or close the file descriptor.
 * * The plugin stub uses some means of IPC to communicate with the plugin
 * server process as if the sever process were a system daemon.
 * * The orderly shutdown plugin server process is initated though an IPC call,
 * not though signals, because Windows doesn't have signals.
 * * The plugin server process will shutdown fast, in under 500 milliseconds.
 * * The plugin server will be reentrant.
 *
 * If your plugin server process cannot exhibit this behavior, create a plugin
 * server process that can exhibit this behavior and have it be the one to
 * manage your aberant plugin server process.
 *
 * ## Difficulties
 *
 * It is difficult to implement process monitoring in fashion that
 *
 * * is portable across Widows and all flavors of UNIX, 
 * * cannot assume control of process monitoring facilities, and
 * * works within a host application that might be surpised by the presence of a
 * child proess that it did not create.
 *
 * A plugin may be loaded into any application that implements the targeted
 * plugin API. This means that your plugin may be loaded into host applications
 * with different appliciation architectures.
 *
 * You might be in a multi-threaded application, where forked proceses can only
 * make async-safe system calls, anything less is thread-unsafe. It is
 * impossible to implement a traditional forked worker when threading is
 * employed. This is why we launch a whole new program and treat it as a daemon,
 * instead of a worker.
 *
 * You might be in an multi-process application that has registered its own
 * signal handlers, and treats your child process as one of its own workers.
 * You don't have control over signal handlers. You don't know who's going to
 * reap your child process.
 * 
 * In both cases, you don't know what file descriptors your plugin worker
 * process will inherit.
 *
 * Of course, none of this applies to Windows. With Windows the problem is that
 * there is no such thing as a child process. A process is isolated from the
 * process that started it entirely. There is no signal handling, and therefore
 * no way to signal an orderly shutdown.
 *
 * In order to reliable use the standard in pipe to send data to the plugin
 * server process on UNIX, the host application must have the signal handler for
 * `SIGPIPE` set to `SIG_IGN`. Writing to the pipe when the plugin process
 * server has closed will cause a `SIGPIPE` signal. If there is no `SIGPIPE`
 * handler in place, then the unhandled `SIGPIPE` will cause the host
 * application to terminate.
 * 
 * If there is a `SIGPIPE` handler in place, then the host application will
 * handle a `SIGPIPE` from a child that it did not create. Testing may determine
 * that this is not an issue, that the registered handler is doing the right
 * thing.
 *
 * OS X offers a per file descriptor flag that disables SIGPIPE, but this is not
 * available on Linux.
 *
 * You can add a test to your plugin where it writes to a pipe with a closed
 * read file descriptor. If the host application disappears when your plugin is
 * loaded, it is not handling `SIGPIPE` gracefully.
 *
 * If this is the case, you should not use the standard in pipe. You can still
 * use read from standard out and standard error. You can still send data to
 * your plugin server process through program arguments. Using program arguments
 * and standard output, you can bootstrap a more robust form of IPC. UNIX domain
 * sockets, for example, can disable `SIGPIPE` on a per socket basis, regardless
 * of the disposition of the `SIGPIPE` signal handlers.
 *
 * Our ideal UNIX host application would set a benign child handler to handle
 * `SIGCHLD`, or leave it the default. It would use `waitpid` to wait only on
 * the exit of child processes that it created itself, allowing the plugin to
 * use `waitpid` to wait on the exit of the plugin process server. It elminate
 * `SIGPIPE` signals by  setting its `SIGPIPE` handler set to the `SIG_IGN`
 * disposition.
 *
 * ## How It Works
 *
 * On UNIX, the plugin server process inherits a file descriptor that is the
 * write end of a pipe. The plugin server process must not write to this file
 * descriptor, nor close it. The file descriptor will close when the plugin
 * server process exits. The plugin attendant listens for the `EPIPE`. When it
 * arrives it knows that the the plugin server process has terminated.
 *
 * The pipe used to detect plugin server process termination is called the
 * canary pipe.
 *
 * This is how we get around a host application that has disabled child signals,
 * or is waiting for all children and intercepting out exit signals.
 *
 * On Windows we have a handle to the plugin server process and we wait for
 * termination normally.
 *
 * ## Life Cycle
 *
 * The plugin attendant implements the following life cycle.
 *
 * These actions take place during library load.
 *
 * &#9824; &nbsp; `initialize` &mdash; The plugin is loaded and the library
 * initailization function calls the plugin attendant `initialize` function.
 *
 * &#9824; &nbsp; `start` &dash; Immediately after calling `initialize`, the
 * plugin stub calls the `start` function to execute what we hope will be the
 * only instance of the plugin server process we'll need.
 *
 * Library load is over. You should return control to the host application now.
 *
 * &#9824; &nbsp; `ready` &dash; Called when the host applications requests
 * service from the plugin. It will block until the plugin server process has
 * started running.
 *
 * We allow the plugin developer to decide what to do when the plugin server
 * process crashes.
 *
 * &#9824; &nbsp; `abend` &dash; Is as plugin developer provided function that
 * is called when the plugin attendant detects that the plugin server process
 * has crashed. The `abend` function can opt to call the `start` function again
 * to restart the plugin server process. If it does not call the `start`
 * function, the plugin attendant goes into a final shutdown state.
 *
 * The plugin stub might be first to detect a problem with the plugin server
 * process, being cut off in the middle of IPC, or else discovering that that
 * the plugin server process, while running is otherwise unresponsive.
 *
 * &#9824; &nbsp; `retry` &dash; Called by the plugin stub after IPC fails. The
 * function blocks until the plugin attendant successfully restarts the plugin
 * server process, or alternatively, the plugin attendant enters the final
 * shutdown state. If the `retry` method return true, IPC should be retryed. IPC
 * should be retryed until `retry` returns false.
 *
 * Shutdown occurs when the host application unloads the plugin library in the
 * library deinitialization function. The plugin attendant does not send a
 * shutdown signal to the plugin server process. Orderly shutdown is initated
 * through IPC between the plugin stub and the plugin server process.
 *
 * &#9824; &nbsp; `shutdown` &mdash; Called by the plugin stub to inform the
 * plugin attendant that the next plugin server process exit is expected.
 *
 * &#9824; &nbsp; `done` &mdash; Called by the plugin stub with a timeout
 * duration to wait for the plugin server process to terminate.
 *
 * &#9824; &nbsp; `scram` &mdash; If the `done` function times out, the plugin
 * stub should call `scram` to forcibly terminate the plugin server process. It
 * can then wait on `done` without a timeout.
 *
 * When the plugin attendant enters the shutdown state, it cannot exit that
 * state. The plugin server process will not run until the plugin library is
 * reloaded by the host application, or the host application is restarted.
 *
 * &#9824; &nbsp; `destroy` &mdash; Called after the plugin attendant has
 * entered the shutdown state, prior the library unload, to release a smidge of
 * memory.
 * 
 * ## Limitations 
 *
 * If the host application does not ignore `SIGPIPE`, the plugin stub cannot
 * rely on the `stdin` pipe to send data to the plugin server process.
 *
 * You could endavour to write a plugin server process that itself will not
 * crash, a minimal plugin server process, that in turn monitors the workhorse
 * plugin server process, but I'm sure you're going to find a few more
 * difficulties at that level in any case.
 *
 * There are limitations to what an out-of-process server can do. If the plugin
 * is supposed to draw a region of a window in a desktop application, for
 * example, the plugin process might be able to render a image to fill the
 * region, but plugin stub would have to implement the actual GDI calls to paint
 * the image into the region on the screen.
 *
 * ## Limitations That Aren't
 *
 * The plugin attendant monitors your process even thought it might not be able
 * to do traditional process monitoring because the host application has
 * employed signals and is waiting on all child processes for its own process
 * monitoring needs. The same plugin attendant API works on both Windows and
 * multiple flavors of UNIX. With these requirements, there are some practical
 * limitations, that ought not truly limit your abilities.
 *
 * &#9824; &nbsp; The plugin attendant monitors a single plugin server process. The
 * plugin attendant has to accomodate a host application that has reserved the
 * process monitoring for itself. Its method of monitoring the plugin server
 * process is dedicated to reducing conflicts between the host application and
 * the plugin server process. It is not the robust, general purpose process
 * monitoring you get with `systemd`, `perl` or `bash`.
 * 
 * If you need to launch multiple processes to service your plugin, then make
 * the first process you launch a process that monitors your flock of processes.
 * Your plugin server process can assume full control of the process monitoring
 * facilities to manage the flock.
 *
 * &#9824; &nbsp; The plugin attendant does its best to be as unobtrusive as possible,
 * but there are some aggressive actions that it cannot survive. The host
 * application must not close our standard I/O pipes, our canary pipe, and it
 * must not signal our process.
 */

/* ## Invocation of the Muse
 *
 * > Now Helicon must needs pour forth for me,
 *  And with her choir Urania must assist me,
 *  To put in verse things difficult to think. 
 */

/* &mdash; */

/* Written in C. */
#ifdef __cplusplus
extern "C" {
#endif 

struct attendant__errors {
    int attendant;
    int system;
};

/* On UNIX a pipe is a file descriptor. On Windows, a pipe a `HANDLE`. */
#ifdef _WIN32
typedef HANDLE attendant__pipe_t;
#else
typedef int attendant__pipe_t;
#endif

struct attendant__initializer {
  void (*starter)(int restart);
  void (*connector)(attendant__pipe_t in, attendant__pipe_t out);
  /* The full path to the plugin attendant relay program. This program will
   * ensure that all file handles are closed and signal handlers reset. It the
   * responsibility of the plugin developer to distribute the relay program and
   * make it available to the plugin attendnat. */
  char relay[FILENAME_MAX];
  /* */
#ifndef _WIN32
  /* The file descriptor number for the plugin server process side of the canary
   * pipe. Must not conflict with the file descriptor numbers assinged to
   * standard I/O by the operating system. The plugin developer has the option
   * to specify the file descriptor number to avoid any conflicts. If you don't
   * care, then make an arbitrary decision. */
  attendant__pipe_t canary;
  /* */
#endif
/* &mdash; */
};

/* There is one instance of the plugin attendant that monitors only one instance
 * of the plugin server process. The plugin attendant functions are contained
 * within a structure which emulates a namespace. The plugin attendant functions
 * are called by referencing them within the structure. An invocation of the
 * plugin attendant function would appear as `attendant.initialize()` in client
 * code.
 */

/* */
struct attendant {
  /* `initialize` &mdash; This must be called to initialize the the monitor
   * before any other functions are called. Call `initialize` during the global
   * initialization of your library, followed by the initial call to `start`.
   *
   * TODO Re-docco.
   */

  /* &#9824; */
  int (*initialize)(struct attendant__initializer *initializer);

  /* `start` &mdash; Called to start the plugin process server when the plugin
   * library is loaded. Also called from within the plugin developer provided
   * abend function to restart the plugin in the event of an early, unexpected
   * restart.
   *
   * The `start` function can be called by the `abend` function, if the `abend`
   * function decides to restart the plugin server process. The `abend` function
   * should only call the `start` function once in response to an abnormal exit.
   *
   * Otherwise, the `start` function should only be called once outside of the
   * `abend` function, to start the plugin process server when the plugin
   * library loads.
   *
   * The abend callback must take precautions to be thread safe. It will be
   * called from a different thread than the thread that performed the first
   * call to the `start` function. If you need to reference variables shared by
   * the rest of your plugin stub, those variables need to be gaurded by a
   * mutex.
   *
   * After the plugin attendant enters the shutdown state, it cannot be
   * restarted. See the `shutdown` function for more details on the shutdown
   * state.
   *
   * TODO Re-docco.
   */

  /* &#9824; */
  int (*start)(
    /* The aboslute path to plugin server program. */
    const char* path,
    /* A null terminated array of program arguments. */
    char const* argv[]
  /* &mdash; */
  );

  /* `ready` &mdash; Called after the initial call to `start` to wait for the
   * plugin server process to start before IPC.
   *
   * The `ready` function should guard any function that could be called by the
   * host application after library load. If your plugin API has a subsequent
   * initialization function after library load, like a plugin object
   * constructor, you can place a call to `ready` there.
   *
   * If the host application can call any plugin function after library load,
   * you'll have to call `ready` at the start of every function. A call to
   * `ready` is not expensive, though, so don't contort your plugin to avoid
   * calling it.
   *
   * The `ready` function blocks until the plugin server process has started.
   * There may still be a race condition where the plugin stub makes a call to
   * the plugin server process over an IPC channel that the plugin server
   * process has yet to initialize. To avoid this race condition, you could use
   * the stdout pipe to listen for an okay of some kind form the plugin server
   * process.
   *
   * The standard I/O pipes will be established when the `ready` function
   * returns true.
   *
   * If the `ready` function returns false the plugin attendant has entered the
   * final shutdown state and the plugin process will never run. This would be
   * due to a catastrophic error, such a missing plugin server program file.
   */

  /* &#9824; */
  int (*ready)();

  /* `retry` &mdash; Report that IPC with the plugin server process failed,
   * indicating that the plugin server process is either crashed or hung.
   *
   * Calling `retry` will cause the plugin attendant to initiate a restart of
   * the plugin server process. It will not initiate a restart if it hasn't
   * already detected the plugin server process failure itself. 
   *
   * The `retry` function is thread-safe. Multiple plugin stub threads can
   * report that the plugin process server is missing or unresponsive. The
   * plugin attendant will only restart the plugin server once.
   *
   * The `retry` function blocks until the plugin attendant successfully
   * restarts the plugin server process, or alternatively, the plugin attendant
   * enters the final shutdown state. If the `retry` function return true, IPC
   * should be retryed. IPC should be retryed until IPC succeeds, or `retry`
   * returns false.
   *
   * If `retry` returns false, the plugin attendant has entered the final
   * shutdown state. The plugin server process will not be restarted. The reason
   * may be obtained using the `errors` function. If the attendant error code is
   * `0`, that indicates that there is no error, and the plugin attendant
   * entered the final shutdown state due to an orderly shutdown. */

  /* &#9824; */
  int (*retry)(int millis);

  /* `shutdown` &mdash; Inform the plugin attendant that shutdown time has come
   * and that the next plugin server process is expected.
   *
   * The plugin attendant does not signal the plugin process server to shutdown
   * itself. The plugin stub must tell the plugin server process to shutdown
   * through some form of IPC.
   *
   * The `shutdown` function only informs the the plugin attendant that it
   * should no longer treat plugin server process exit as abnormal. That is
   * should no longer attempt to restart the plugin server process.
   *
   * Once in the shutdown state, the plugin server process cannot be restarted
   * by the currently loaded plugin library. The plugin server process will not
   * run again until the plugin library is reloaded or the host application is
   * restrated.
   *
   * The plugin attendant will enter the shutdown state in response to a call to
   * the `shutdown` function, the `scram` function, or if the plugin developer
   * supplied `abend` function does not call the `start` function to restart the
   * server.
   *
   * Additionally, the plugin attendant will enter the shutdown state in
   * response to failed assertions. If the `start` function fails, or `retry`
   * function fails, you should inspect the errors using the `errors` function,
   * record them, and tell me about them.
   *
   * Returns false if the plugin server process shutdown and will not run again.
   * Returns true if the plugin server process is running, or not running but
   * potentially restarting.
   */

  /* &#9824; */
  int (*shutdown)();

  /* `done` &mdash; Blocks until the server process terminates or the timeout
   * expires. Call this function after an orderly shutdown or a call to `scram`.
   * 
   * An orderly shutdown consists of calling the `shutdown` function, then
   * telling the plugin server process to shutdown through IPC. An orderly
   * shutdown should be performed when the plugin library is unloaded and the
   * library deinitialization function is called.
   *
   * After initiating an orderly shutdown, wait on `done` with a resonable
   * timeout. If the timeout expires, `done` will return false.
   *
   * If `done` returns false you should force plugin server termination with by
   * calling the `scram` function.
   */

  /* &#9824; */
  int (*done)(int milliseconds);

  /* `scram` &mdash; We give you a way to obliterate the process here, this
   * triggers a hard shutdown of the process using harsh methods to pull the rug
   * out from under your server process. Your server process will not have a
   * chance to call any cleanup routines, it will just disappear. This is a last
   * resort.
   */

  /* &#9824; */
  int (*scram)();

  /* `errors` &mdash; Returns an integer array of two elements relating to the
   * last error encountered by the plugin attendant. The first element is a
   * plugin attendant error code.
   *
   * Errors can be checked after a failed call to one of the plugin attendant
   * function, or else at the start of a call to the plugin developer suppliesd
   * `abend` function, before any plugin attendant functions are called, to
   * obtain the reason why the abend function was called.
   *
   * Errors generated by the problems with the plugin attendant itself are
   * really assertions. The plugin attendant has so few moving parts that if it
   * cannot function, there is something wrong with configuration or
   * instalation of the plugin.
   *
   * Errors indicating that the plugin process cannot start would also indicate
   * that there is a problem with configuration or installation.
   *
   * When the plugin server process crashes, the `errors` function will give a
   * a reason so vauge as to be useless. The plugin server process should
   * maintain a log that can be referenced to see where it terminated, or a
   * crash log that records the stack before exit. Using a crash reporter, and
   * with the end user's permission, you can use this log to diagnose errors.
   *
   * There are a few errors that are marked as assertions. If you detect one of
   * these errors, then I've made an assumption about how the plugin attendant
   * is supposed to work that your experience in the field has proven false.
   *
   * Please inform me of these assertions if they arise.
   */

  /* &#9824; */
  struct attendant__errors (*errors)();

  /* `destroy` &mdash; Called after the plugin process server has shutdown, the
   * plugin attendant has entered the shutdown state. Call this function after
   * shutdown to free a few resources before the library is unloaded.
   */

  /* &#9824; */
  int (*destroy)();

  /* */
};

/* The one and only plugin attendant. */
extern struct attendant attendant;

/* Love, C. */
#ifdef __cplusplus
}
#endif
