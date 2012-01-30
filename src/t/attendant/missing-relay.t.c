#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../../../errors.h"
#include "../../../attendant.h"
#include "../ok.h"
#include "../../../eintr.h"

void abend() {
  struct attendant__errors errors = attendant.errors();
  ok(errors.attendant == START_CANNOT_EXECV, "start cannot execve");
  ok(errors.system == ENOENT, "enoent");
}

int main() {
  char path[PATH_MAX], **trace;
  int err;
  char const * argv[] = { NULL };

  printf("1..2\n");
  attendant.initialize(strcat(getcwd(path, PATH_MAX), "/relay-x"), 31);  
  err = attendant.start(strcat(getcwd(path, PATH_MAX), "/t/bin/server"), argv, abend);
  attendant.ready();
  attendant.destroy();  

  return EXIT_SUCCESS;
}
