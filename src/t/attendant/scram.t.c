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
  "[reap/poll]",
  "[ready/exit]",
  "[reap/shutdown]",
  "[reap/poll]",
  "[shutdown/exit]",
  "[done/exit]", 
  "[shutdown/exit]",
  "[scram/initiated]",
  "[reap/shutdown]",
  "[reap/poll]",
  "[reap/instance]",
  "[reap/poll]",
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
  char path[PATH_MAX], **trace;
  char const * argv[] = { NULL };

  printf("1..3\n");

  attendant.initialize(strcat(getcwd(path, PATH_MAX), "/relay"), 31);  
  attendant.start(strcat(getcwd(path, PATH_MAX), "/t/bin/server"), argv, abend);
  attendant.ready();
  attendant.shutdown();
  ok(! attendant.done(250), "can't exit");
  attendant.scram();
  ok(attendant.done(-1), "done");

  trace = attendant.tracepoints();
  trace_ok(trace, expected, "scram");

  attendant.destroy();  

  return EXIT_SUCCESS;
}
