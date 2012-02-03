#include <windows.h>
#include <stdlib.h>

int main() {
  MessageBoxA(GetDesktopWindow(), "Foo", "Foo", 0);
  return EXIT_SUCCESS;
}
