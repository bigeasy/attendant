#include <stdio.h>
#include <Windows.h>
#include <stdlib.h>
#include "attendant.h"

int main() {
    attendant.initialize(NULL, 0);
    attendant.start(NULL, NULL, NULL);
    attendant.destroy();
    return EXIT_SUCCESS;
}
