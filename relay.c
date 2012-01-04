/* There is a lot of work to do to create clean slate for our server process,
 * and the easiest way to get there is to have an intremediate program occupy
 * the process.
 *
 * Initially, I tried to do this was going to take place in the attendant
 * library start up function, linked to the host application, after fork and
 * before execve. That whole block of code felt like a race condition.
 *
 * By stepping on this stepping stone, we do our setup in an exec'd program in a
 * far less fragile state that a fork within a multi-threaded program.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>

/* Contains error code that will be written to stderr and read by a startup
 * thread running in the host application launched by the attendant library. */
#include "errors.h"
#include "eintr.h"

/* The first argument to the program is file handle of a pipe used to report
 * errors to the library proecess start thread in the host application. We use
 * this extra pipe instead of STDERR or STDOUT because it will be closed on
 * exec. We set all non-stdio file descriptrs to close on exit so that the
 * server process will not inherit file descriptors from the host application.
 * The start thread blocks on the extra pipe reading a error code an errno
 * written using the send_error function. If it gets an EPIPE instead of an
 * error code, it knows that the relay program is done and the server program
 * has exec'd, so it knows that there were no errors in the relay program.
 */
static int spipe;

/* True if a file handle is a stdio file handle. */
int is_stdio(int fd) {
  return fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO;
}

/* When the host application is a multi-threaded application, many for the
 * standard library functions are unavailable between fork and execvp. We cannot
 * call malloc for example. That presents a problem for us in our file handler
 * cleanup code.
 *
 * The only reliable way to get a list of file handles from Linux or OS X is to
 * do a directory listing of "/dev/fd". The `opendir` function calls malloc.
 * Thus, we cannot do a directory listing between fork and execve.
 *
 * We could do a directory listing before we fork, but now we have a race
 * condition. After fork, the set of open file handles is fixed. We have a copy
 * of all the file handles in the parent, but after fork, if the parent opens a
 * file, the forked child will not see it. The child will only see the file
 * handles that it opens itself.
 *
 * Before we fork, we can create a list of open file descriptors in an array,
 * but from the moment we're done reading the directory listing of "/dev/fd", to
 * the moment we fork, another thread may open a new file.
 *
 * I got the point that I was thinking that the library server process would
 * want to double check that an inherited open file handle was skipped. From
 * there I realized that I might as well load an intermediate program that does
 * this cleanup. Do it once and do it right. 
 *
 * Furthermore, the well-behaved host application is going to set FD_CLOEXEC
 * flag on the file it creates. This would close those file descriptors in the
 * child process when execve is called in the startup thread. The set of
 * inherited file descriptors after fork is every open file descriptor in the
 * host application. Doing the file descriptor cleanup here, after execve, will
 * probably mean fewer file descriptors to close.
 *
 * Rather than closing the file descriptors, we set the close on exec flag
 * FD_CLOEXEC. This is because the attendent library in the host application has
 * fed us a senty file descriptor that is the write end of a pipe. When we set
 * FD_CLOEXEC on that file descriptor and then execve, a read from the read end
 * of the pipe will generate and EPIPE. This program will send a success code
 * through STDERR prior to execve. If the startup thread recieves the success
 * code, it reads from the senty pipe until it gets an EPIPE. At that point it
 * knows that the server program has been loaded into the process.
 */
void set_close_on_exec() {
  DIR *dir;
  struct dirent *ent;
  int fd;

  dir = opendir("/dev/fd");
  if (dir == NULL) {
    send_error(spipe, RELAY_CANNOT_OPEN_DEV_FD);
  } else {
    while ((ent = readdir(dir)) != NULL) {
      fd = atoi(ent->d_name);
      if (! is_stdio(fd) && fd != dirfd(dir)) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
      }
    }
    closedir(dir);
  }
}

/* Signals need to be returned to their default disposition. When forked, the
 * child process will inherit signal handlers and their settings. After an
 * execve, however, signals are reset to their default disposition, except for
 * those signals that the host application chose to ignore. Ignored signals in
 * the parent process are also ignored in the exceve process.
 * 
 * We check to see if any signal handlers are set to SIG_IGN. If so, we return
 * them to the default SIG_DFL. The server program will then have a blank slate
 * of signal handlers.
 */

/* We don't gaurd against a host application that considers us an attacker. The
 * library process attendent is linked into a dynamically loaded library
 * implemented to extend the an application that provides a plugin mechanism.
 * The host application will have loaded the plugin. The host application should
 * expect the plugin to open files, sockets, etc.
 * 
 * This is because the attendent library is dynamically loaded into
 * a host application, where the library has no control over the global
 * environment. It cannot make assumptions about the disposition of signal han
 
 * This process is going to close down any file handles left open
 * from out library launcher. It doesn't have any restrictions on signal
 * handlers, memory allocation, file handles, threads. It can use what ever
 * resources it wants, close things up, and then execve.
 */
int reset_signals() {
  struct sigaction sig;
  int signum;
  for (signum = SIGHUP; signum < NSIG; signum++) {
    sigaction(signum, NULL, &sig);
    if (sig.sa_handler == SIG_IGN) {
      sig.sa_handler = SIG_DFL;
      sigaction(signum, &sig, NULL);
    }
  }
}

/* The first argument is our status pipe, so me make sure it is alive.
 * If we didn't even get a first argument, we just exit.  If we did, we convert
 * it and write to stdout.  The start thread will listen for this and assert
 * that we're going to use the correct pipe for status reporting.  If it doesn't
 * get the correct fd on stdout, it knows that we weren't able to get past this
 * step. If we send the wrong fd, then the start thread can record the error and
 * give up on launching the server process altogether, because that would be a
 * really fundimental error. */
void get_status_pipe(int argc, char *argv[]) {
  int err;
  if (argc < 2) exit(127);
  spipe = atoi(argv[1]);
  if (spipe == 0) {
    /* Can't be right. */
    exit(127);
  }
  HANDLE_EINTR(write(STDOUT_FILENO, &spipe, sizeof(spipe)), err);
  if (err == -1) {
    exit(127);
  }
  /* Now we prove to the start thread that the pipe is working by sending the
   * pipe fd back trough the pipe itself. */
  HANDLE_EINTR(write(spipe, &spipe, sizeof(spipe)), err);
  if (err == -1) {
    exit(127);
  }
  /* Now we know that error reporting will work correctly, so check that we haev
   * a program to run specified by an absolute path before we go one. */
}

/* We check to see that we received a program name and that the path is
 * absolute. We'll let execl determine if the program does actually exist as an
 * executable on the filesystem.
 */
void verify_arguments(int argc, char *argv[]) {
  if (argc < 3) {
    send_error(spipe, RELAY_PROGRAM_MISSING);
  }
  if (argv[2][0] != '/') {
    send_error(spipe, RELAY_PROGRAM_PATH_NOT_ABSOLUTE);
  }
}

void execute(int argc, char *argv[]) {
  char *args[ARG_MAX];
  char *path = argv[2];
  int i;

  args[0] = path;
  for (i = 0; i < argc - 3; i++) {
    args[i + 1] = argv[i + 3];
  }
  args[argc - 1] = NULL;
  /*
  fprintf(stderr, "XX %s\n", path);
  for (i = 0; args[i]; i++) {
    fprintf(stderr, "YY %s\n", args[i]);
  }
  */

  /* Obliterate ourselves. */
  execv(path, args);

  /* If we are here then execl failed. */
  send_error(spipe, RELAY_CANNOT_EXEC);
}

/* Perform the cleaning operations and execve the server process. */
int main(int argc, char *argv[]) {
  get_status_pipe(argc, argv);

  /* Check that the arguments are as expected. */
  verify_arguments(argc, argv);

  /* Reset signals. */
  reset_signals();

  /* Set all non-stdio file handles to close on exec. */
  set_close_on_exec();

  /* Call execve to replace this relay program with the server program. */
  execute(argc, argv);

  /* This program ought to have been replaced by the library server program 
   * by now. If not, it would have sent an error and exited. If we are here, we
   * have failed and failed at failing. The shame. It burns. */
  return EXIT_FAILURE;
}
