#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "metal/tty.h"
#include "metal/io.h"

#define LPRINTF(_f_, _l_, _msg_, ...) \
    printf("%s[%d] " _msg_ "\n", _f_, _l_, ##__VA_ARGS__)
#define PRINTF(_msg_, ...) \
    LPRINTF(__func__, __LINE__, _msg_, ##__VA_ARGS__)

static void _hello(void)
{
    printf("Hello, World!\n");
}

int main(int argc, char * argv[])
{
    for(unsigned int ix=0; ix<argc; ix++) {
        printf("argv[%u] = {%s}\n", ix, argv[ix]);
    }

    _hello();

    return 0;
}
