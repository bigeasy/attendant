#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "../ok.h"
#include "../reset.h"

int main() {
  struct sigaction sig;

  printf("1..1\n");

  memset(&sig, 0, sizeof(struct sigaction));
  sig.sa_handler = SIG_IGN;
  sigaction(SIGALRM, &sig, NULL);

  check_reset("signal disposition reset");

  return EXIT_SUCCESS;
}
