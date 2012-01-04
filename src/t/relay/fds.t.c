#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ok.h"
#include "../reset.h"

int main() {
  int err, tunnel[2];

  printf("1..1\n");

  CHECK(err = pipe(tunnel), err == -1);

  check_reset("file descriptors reset");

  return EXIT_SUCCESS;
}
