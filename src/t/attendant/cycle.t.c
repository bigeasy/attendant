#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "../../../attendant.h"
#include "../ok.h"
#include "../../../eintr.h"

int count = 0;
void starter(int restart) {
  char path[PATH_MAX];
  char const * argv[] = { NULL };
  if (count++ < 1) {
    attendant.start(strcat(getcwd(path, PATH_MAX), "/t/bin/when"), argv);
  }
}

static char fifo[PATH_MAX];

void connector(attendant__pipe_t in, attendant__pipe_t out) {
  const char *pipe = "pipe\n";
  int err;
  HANDLE_EINTR(write(in, pipe, strlen(pipe)), err);
  ok(err != -1, "request pipe");
  HANDLE_EINTR(read(out, fifo, sizeof(fifo)), err);
  fifo[strlen(fifo) - 1] = '\0';
  ok(err != -1, "get pipe # %s", fifo);
}

int main() {
  int err, fd;
  char const * exit = "exit\n";
  struct attendant__initializer initializer;

  initializer.starter = starter;
  initializer.connector = connector;
  strcat(getcwd(initializer.relay, sizeof(initializer.relay)), "/relay");
  initializer.canary = 31;

  printf("1..4\n");

  attendant.initialize(&initializer);
  starter(0);
  attendant.ready();
  attendant.shutdown();

  fd = open(fifo, O_WRONLY);
  ok(fd != -1, "open fifo");
  fflush(stdout);
  HANDLE_EINTR(write(fd, exit, strlen(exit)), err);
  ok(err != -1, "write fifo");

  attendant.destroy();  

  return EXIT_SUCCESS;
}
