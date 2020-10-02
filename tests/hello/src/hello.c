#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "metal/machine.h"
#include "metal/tty.h"

#define LPRINTF(_f_, _l_, _msg_, ...) \
    printf("%s[%d] " _msg_ "\n", _f_, _l_, ##__VA_ARGS__)
#define PRINTF(_msg_, ...) \
    LPRINTF(__func__, __LINE__, _msg_, ##__VA_ARGS__)

int main(void) {

    PRINTF("Hello, World!\n");

    return 0;
}
