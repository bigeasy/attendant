#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "errors.h"

void send_error(int fd, int code) {
  int message[2];

  message[0] = code;
  message[1] = errno;
  write(fd, message, sizeof(message));

  _exit(EXIT_FAILURE);
}
