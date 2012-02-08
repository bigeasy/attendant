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

void starter(int restart) {
  struct attendant__errors errors = attendant.errors();
  ok(errors.attendant == RELAY_CANNOT_EXEC, "relay cannot exec");
  ok(errors.system == ENOENT, "enoent");
}

int main() {
  char path[PATH_MAX];
  char const * argv[] = { NULL };
  struct attendant__initializer initializer;

  initializer.starter = starter;
  strcat(getcwd(initializer.relay, PATH_MAX), "/relay-x");
  initializer.canary = 31;

  printf("1..2\n");
  attendant.initialize(&initializer);
  attendant.start(strcat(getcwd(path, PATH_MAX), "/t/bin/server-x"), argv);
  attendant.ready();
  attendant.destroy();  

  return EXIT_SUCCESS;
}
