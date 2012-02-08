#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../../../attendant.h"
#include "../ok.h"
#include "../../../eintr.h"

static int count = 0;

void starter(int restart) {
  char path[PATH_MAX];
  char const * argv[] = { NULL };
  if (count++ < 3) {
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

struct retry {
  const char* name;
  int count;
};

void send(const char *message) {
  int fd, err;
  fd = open(fifo, O_WRONLY);
  if (fd == -1) {
    bail("cannot open fifo");
  }
  HANDLE_EINTR(write(fd, message, strlen(message)), err);
  if (err == -1) {
    bail("cannot write fifo");
  }
  err = close(fd);
  if (err == -1) {
    bail("cannot close fifo");
  }
}

void recv(char *buffer, int length) {
  int fd, err;
  fd = open(fifo, O_RDONLY);
  if (fd == -1) {
    bail("cannot open fifo for read");
  }
  HANDLE_EINTR(read(fd, buffer, length), err);
  if (err == -1) {
    bail("cannot write fifo");
  }
  err = close(fd);
  if (err == -1) {
    bail("cannot close fifo");
  }
}

/* We run this thread twice. The second time will cause the retry loop to fire
 * more than twice. */
static void* retry(void *data) {
  struct retry *retry = (struct retry*) data;
  const char* name = retry->name;
  int count = 0;
  char first[64], second[64], *ping = "ping\n";

  /* Ping the plugin server process to get its start time. */
  send(ping);
  memset(first, 0, sizeof(first));
  recv(first, sizeof(first));

  /* Ping the plugin server process to get its start time again. */
  send(ping);
  memset(second, 0, sizeof(second));
  recv(second, sizeof(second));

  fprintf(stderr, "Starting.\n");

  /* Sanity check to ensure that the server process correctly reports the same
   * start time when it runs uninterrupted. */
  ok(strcmp(first, second) == 0, "running from %s thread", name);

  while (attendant.ready() && strcmp(first, second) == 0) {
    count++;
    fprintf(stderr, "Retry %d.\n", count);
    attendant.retry(1000);
    send(ping);
    memset(second, 0, sizeof(second));
    recv(second, sizeof(second));
    fprintf(stderr, "%s", second);
  }

  /* By now we should have restarted. */
  ok(strcmp(first, second) != 0, "restarted from %s thread", name);
  ok(count == retry->count, "retry count correct in %s thread %d", name, count);

  return NULL;
}

/* Run the above test twice, in two separate threads. The second run ought to
 * call retry twice, because its instance number will be less than the process
 * instance number. */
int main() {
  struct retry expected;
  const char *term = "exit\n";
  pthread_t thread;
  struct attendant__initializer initializer;

  printf("1..14\n");

  initializer.starter = starter;
  initializer.connector = connector;
  strcat(getcwd(initializer.relay, sizeof(initializer.relay)), "/relay");
  initializer.canary = 31;

  /* Initialize the attendant. */
  attendant.initialize(&initializer);

  /* Start the server. */
  starter(0);
  attendant.ready();

  expected.name = "first";
  expected.count = 1;
  retry(&expected);

  expected.name = "second";
  expected.count++;
  pthread_create(&thread, NULL, retry, &expected);
  pthread_join(thread, NULL);

  /* Shutdown the server. */
  attendant.shutdown();
  send(term);

  /* Wait for shutdown. */
  ok(attendant.done(1000), "done");

  /* Destroy the attendant. */
  attendant.destroy();  

  /* We're good. */
  return EXIT_SUCCESS;
}
