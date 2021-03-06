#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "ok.h"

void ok(int cond, char const *format, ...) {
  char const *not = cond ? "" : "not ";
  char buffer[4096];
  va_list args;
  va_start(args,format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  printf("%sok %s\n", not, buffer);
}

void bail(const char* message) {
  printf("Bail out! %s %s\n", message, strerror(errno));
  exit(EXIT_FAILURE);
}
