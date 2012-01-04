#include <stdio.h>
#include <stdlib.h>

#include "ok.h"

void ok(int cond, char const *message) {
  char const *not = cond ? "" : "not ";
  printf("%sok %s\n", not, message);
}

void bail(const char* message) {
  printf("Bail Out! %s\n", message);
  exit(EXIT_FAILURE);
}
