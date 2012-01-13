#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../../../attendant.h"
#include "../ok.h"
#include "../../../eintr.h"

void abend() {
  char path[PATH_MAX];
  char const * argv[] = { NULL };
  attendant.start(strcat(getcwd(path, PATH_MAX), "/t/bin/server"), argv, abend);
}

int main() {
  int err;
  char path[PATH_MAX], **trace, ch = '\n';
  char const * argv[] = { NULL };

  printf("1..2\n");

  attendant.initialize(strcat(getcwd(path, PATH_MAX), "/relay"), 31);  
  attendant.start(strcat(getcwd(path, PATH_MAX), "/t/bin/crasher"), argv, abend);
  attendant.ready();
  trace = attendant.tracepoints();
  HANDLE_EINTR(read(attendant.stdio(1), &ch, sizeof(ch)), err);
  ok(attendant.ready(), "restarted");
  attendant.shutdown();
  HANDLE_EINTR(write(attendant.stdio(0), &ch, sizeof(ch)), err);
  ok(attendant.done(30000), "done");

  attendant.destroy();  

  return EXIT_SUCCESS;
}
