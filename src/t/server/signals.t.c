#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "../ok.h"

int test_fork_directly_with_sig_ign() {
  struct sigaction sig;
  int pipein[2], pipeout[2], err;
  char ch = '\n', line[128], *ptr;
  pid_t pid;
  FILE *out;

  memset(&sig, 0, sizeof(struct sigaction));
  sig.sa_handler = SIG_IGN;
  sigaction(SIGALRM, &sig, NULL);

  CHECK(err = pipe(pipein), err == -1);
  CHECK(err = pipe(pipeout), err == -1);

  CHECK(err = fcntl(pipein[1], F_SETFD, FD_CLOEXEC), err == -1);
  CHECK(err = fcntl(pipeout[0], F_SETFD, FD_CLOEXEC), err == -1);

  pid = fork();
  if (pid == 0) {
    CHECK(err = dup2(pipein[0], STDIN_FILENO), err == -1);
    CHECK(err = dup2(pipeout[1], STDOUT_FILENO), err == -1);
    CHECK(err = close(pipein[0]), err == -1);
    CHECK(err = close(pipeout[1]), err == -1);
    execl("t/bin/server", "t/bin/server", (char*) 0);
    bail(strerror(errno));
  } else if (pid != 0) {
    CHECK(out = fdopen(pipeout[0], "r"), !out);
    CHECK(ptr = fgets(line, sizeof(line), out), !ptr); 
    ok(strstr(line, "SIGNAL 14") == line, "signal disposition inherited");
    CHECK(err = write(pipein[1], &ch, sizeof(ch)), err == -1);
    CHECK(waitpid(pid, 0, 0), err == -1);

    CHECK(err = close(pipein[1]), err == -1);
    CHECK(err = fclose(out), err == -1);
  } else {
    printf("# Cannot fork.\n");
    exit(EXIT_FAILURE);
  }
}

int main() {
  printf("1..1\n");
  test_fork_directly_with_sig_ign();
}
