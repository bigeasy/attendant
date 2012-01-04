#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/* True if a file handle is open. */
int is_open(int fd) {
  return fcntl(fd, F_GETFL) != -1 || errno != EBADF;
}

/* True if a file handle is a stdio file handle. */
int is_stdio(int fd) {
  return fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO;
}

/* This is a testing server. It quits when it gets any input on stdin. It
 * reports any unexpected conditions to standard out. It verifies that the
 * attendant has started the process with a clean environment. It represents a
 * well behaved library server process. */
int main() {
  struct sigaction sig;
  int signum, fd;
  char ch;
  DIR *dir;
  struct dirent *ent;

  /* Report any signal that is not set to the default signal disposition. */
  for (signum = SIGHUP; signum < NSIG; signum++) {
    sigaction(signum, NULL, &sig);
    if (sig.sa_handler != SIG_DFL) {
      printf("SIGNAL %d: %s.\n", signum, strsignal(signum));
      fflush(stdout);
    }
  }

  /* Report any open file handles other than stdio handles. */
  dir = opendir("/dev/fd");
  if (dir == NULL) {
    printf("FDS: Cannot open /dev/fd.");
  } else {
    while ((ent = readdir(dir)) != NULL) {
      fd = atoi(ent->d_name);
      if (! is_stdio(fd) && fd != dirfd(dir)) {
        printf("OPEN: %d.\n", fd);
      }
    }
    closedir(dir);
  }

  /* Report any stdio handles that are closed. */
  if (! is_open(STDIN_FILENO))  printf("CLOSED: STDIN");
  if (! is_open(STDOUT_FILENO)) printf("CLOSED: STDOUT");
  if (! is_open(STDERR_FILENO)) printf("CLOSED: STDERR");

  printf("RUNNING\n");
  fflush(stdout);

  /* Termiante on any input. */
  putchar(getchar());

  /* Always successful, even if things weren't as expected, we did our job in
   * reporting the state. If it is wrong, that is someone else's failure. */
  return EXIT_SUCCESS;
}
