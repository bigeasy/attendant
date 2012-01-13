#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "eintr.h"
#include "errors.h"

void send_error(int fd, int code) {
  int message[2], err;

  message[0] = code;
  message[1] = errno;
  HANDLE_EINTR(write(fd, message, sizeof(message)), err);

  _exit(EXIT_FAILURE);
}
