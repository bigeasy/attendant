#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "ok.h"

int check_reset(const char *message) {
  int pipein[2], pipeout[2], pipestat[2], spipe, err;
  char number[16], path[PATH_MAX], ch = '\n', line[128], *ptr;
  FILE *out;
  pid_t pid;

  CHECK(err = pipe(pipein), err == -1);
  CHECK(err = pipe(pipestat), err == -1);
  CHECK(err = pipe(pipeout), err == -1);

  CHECK(err = fcntl(pipein[1], F_SETFD, FD_CLOEXEC), err == -1);
  CHECK(err = fcntl(pipestat[0], F_SETFD, FD_CLOEXEC), err == -1);
  CHECK(err = fcntl(pipeout[0], F_SETFD, FD_CLOEXEC), err == -1);

  sprintf(number, "%d", pipestat[1]);
  strcat(getcwd(path, PATH_MAX), "/t/bin/server");
  
  switch (pid = fork()) {
  case -1:
    printf("# Cannot fork.\n");
    exit(EXIT_FAILURE);
  case 0:
    CHECK(err = dup2(pipein[0], STDIN_FILENO), err == -1);
    CHECK(err = dup2(pipeout[1], STDOUT_FILENO), err == -1);

    CHECK(err = close(pipein[0]), err == -1);
    CHECK(err = close(pipeout[1]), err == -1);

    execl("relay", "relay", number, "1", path, (char*) 0);
    bail("cannot exec relay");
  default:
    CHECK(err = read(pipeout[0], &spipe, sizeof(spipe)), err == -1);
    if (spipe != pipestat[1]) bail("status pipe not received");

    CHECK(err = read(pipestat[0], &spipe, sizeof(spipe)), err == -1 && spipe == pipestat[1]);
    if (spipe != pipestat[1]) bail("status pipe not received");

    CHECK(out = fdopen(pipeout[0], "r"), !out);
    CHECK(ptr = fgets(line, sizeof(line), out), !ptr); 
    
    CHECK(err = write(pipein[1], &ch, sizeof(ch)), err == -1);
    CHECK(err = waitpid(pid, 0, 0), err == -1);

    ok(strstr(line, "RUNNING") == line, message);

    CHECK(err = close(pipein[1]), err == -1);
    CHECK(err = fclose(out), err == -1);
  }

  return EXIT_SUCCESS;
}
