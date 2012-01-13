#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../../../attendant.h"
#include "../ok.h"
#include "../../../eintr.h"

void abend() { }

static char * expected[] = {
  "[initialize/success]",
  "[start/success]",
  "[launch/fork]",
  "[launch/success]",
  "[launch/poll]",
  "[ready/exit]",
  "[reap/shutdown]",
  "[launch/poll]",
  "[shutdown/exit]",
  "[reap/hangup]",
  "[reap/hungup]",
  "[done/exit]",
  NULL
};

void trace_ok(char **trace, char **expected, char *message) {
  int matched = 1, i;
  for (i = 0; matched && trace[i]; i++) {
    if ((matched = ! ! expected[i])) {
      matched = (strcmp(trace[i], expected[i]) == 0);
    } 
  }

  matched = matched && ! expected[i];

  ok(matched, message);
  
  if (! matched) {
    printf("# EXPECTED:\n");
    for (i = 0; expected[i]; i++) {
      printf("# %s\n", expected[i]);
    }
    printf("# GOT:\n");
    for (i = 0; trace[i]; i++) {
      printf("# %s\n", trace[i]);
    }
  }
}

int main() {
  int err;
  char path[PATH_MAX], **trace, ch = '\n';
  char const * argv[] = { NULL };

  printf("1..2\n");

  attendant.initialize(strcat(getcwd(path, PATH_MAX), "/relay"), 31);  
  attendant.start(strcat(getcwd(path, PATH_MAX), "/t/bin/server"), argv, abend);
  attendant.ready();
  attendant.shutdown();
  HANDLE_EINTR(write(attendant.stdio(0), &ch, sizeof(ch)), err);
  ok(attendant.done(30000), "done");

  trace = attendant.tracepoints();
  trace_ok(trace, expected, "scram");

  attendant.destroy();  

  return EXIT_SUCCESS;
}
