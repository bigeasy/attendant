#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../../../attendant.h"
#include "../ok.h"
#include "../../../eintr.h"

/* The server process restarter. */
void abend() {
  char path[PATH_MAX];
  char const * argv[] = { NULL };
  attendant.start(strcat(getcwd(path, PATH_MAX), "/t/bin/when"), argv, abend);
}

/* Report ok with a formatted message. */
int okay(int condition, const char* format, ...) {
  char buffer[4096];
  va_list args;
  va_start(args,format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  ok(condition, buffer);
}

struct retry {
  const char* name;
  int count;
};

/* We run this thread twice. The second time will cause the retry loop to fire
 * more than twice. */
static void* retry(void *data) {
  struct retry *retry = (struct retry*) data;
  const char* name = retry->name;
  int count = 0, pinged = 0, err;
  char first[64], second[64], *ping = "ping\n", *close = "close\n";

  /* Ping the plugin server process to get its start time. */
  HANDLE_EINTR(write(attendant.stdio(0), ping, strlen(ping)), err);
  okay(err != -1, "write ping of %s thread", name);
  memset(first, 0, sizeof(first));
  HANDLE_EINTR(read(attendant.stdio(1), first, sizeof(first)), err);
  okay(err != -1, "read ping of %s thread", name);

  /* Ping the plugin server process to get its start time again. */
  HANDLE_EINTR(write(attendant.stdio(0), ping, strlen(ping)), err);
  okay(err != -1, "write confirm ping of %s thread", name);
  memset(second, 0, sizeof(second));
  HANDLE_EINTR(read(attendant.stdio(1), second, sizeof(second)), err);
  okay(err != -1, "read confirm ping of %s thread", name);

  /* Sanity check to ensure that the server process correctly reports the same
   * start time when it runs uninterrupted. */
  okay(strcmp(first, second) == 0, "running from %s thread", name);

  /* Tell the server to close stdandard out. */
  HANDLE_EINTR(write(attendant.stdio(0), close, strlen(close)), err);

  /* Sleep to ensure that the close actually happens. */
  sleep(1);

  /* Now simulate IPC using standard I/O. In a real plugin, we'd use an
   * alternative form of IPC.
   * 
   * Here we run the above ping, but against a server process that closed
   * standard out. We will get an `EPIPE`. The attendant will not know that IPC
   * failed, it can only detect program exit. We will detect that IPC has failed
   * and request that we force a restart of the server process.
   */
  while (attendant.ready() && ! pinged) {
    count++;
    HANDLE_EINTR(write(attendant.stdio(0), ping, strlen(ping)), err);
    if (err == -1) {
      if (errno == EPIPE) {
        attendant.retry(1000);
        continue;
      }
      printf("Bail out! Can't write to server.");
      exit(1);
    }
    memset(second, 0, sizeof(second));
    HANDLE_EINTR(read(attendant.stdio(1), second, sizeof(second)), err);
    if (err == 0) {
      attendant.retry(1000);
    } else {
      pinged = 1;
    }
  }

  /* By now we should have restarted. */
  okay(strcmp(first, second) != 0, "restarted from %s thread", name);
  okay(count == retry->count, "retry count correct in %s thread", name);
}

/* Run the above test twice, in two separate threads. The second run ought to
 * call retry twice, because its instance number will be less than the process
 * instance number. */
int main() {
  int err;
  struct retry expected;
  char path[PATH_MAX], *term = "exit\n";
  char const * argv[] = { NULL };
  struct sigaction sigpipe;
  pthread_t thread;

  /* We might write to a closed pipe when we kill the server process. We set
   * `SIGPIPE` to ignore so that writing to a closed pipe will return `EIPE`
   * instead of generating a signal. */
  memset(&sigpipe, 0, sizeof(sigpipe));
  sigpipe.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sigpipe, NULL);

  printf("1..14\n");

  /* Initialize the attendant. */
  attendant.initialize(strcat(getcwd(path, PATH_MAX), "/relay"), 31);  

  /* Start the server. */
  abend();
  attendant.ready();

  expected.name = "first";
  expected.count = 2;
  retry(&expected);

  expected.name = "second";
  expected.count++;
  pthread_create(&thread, NULL, retry, &expected);
  pthread_join(thread, NULL);

  /* Shutdown the server. */
  attendant.shutdown();
  HANDLE_EINTR(write(attendant.stdio(0), term, strlen(term)), err);
  ok(err != -1, "send exit");

  /* Wait for shutdown. */
  ok(attendant.done(1000), "done");

  /* Destroy the attendant. */
  attendant.destroy();  

  /* We're good. */
  return EXIT_SUCCESS;
}
