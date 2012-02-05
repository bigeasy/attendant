#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main() {
  char buffer[1024], *got;
  time_t now = time(NULL);
  MessageBox(GetDesktopWindow(), "called", "called", 0);
  printf("start\n");
  fflush(stdout);
  while ((got = fgets(buffer, sizeof(buffer), stdin)) != NULL) {
    MessageBox(GetDesktopWindow(), "got", "got", 0);
    if (strcmp(buffer, "when\n") == 0) {
      printf("%ld\n", now);
      MessageBox(GetDesktopWindow(), "when", "when", 0);
    } else if (strcmp(buffer, "echo\n") == 0) {
      printf("echo\n");
      MessageBox(GetDesktopWindow(), "echo", "echo", 0);
    } else if (strcmp(buffer, "exit\n") == 0) {
      break;
    }
    fflush(stdout);
  }
  MessageBox(GetDesktopWindow(), "exit", "exit", 0);
  return EXIT_SUCCESS;
}
