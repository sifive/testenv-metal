#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "metal/machine.h"
#include "metal/tty.h"
#include "api/hardware/v0.5/random/hca_trng.h"
#include "api/hardware/v0.5/sifive_hca-0.5.x.h"
#include "api/hardware/hca_utils.h"
#include "api/hardware/hca_macro.h"

#define UART0 __METAL_DT_SERIAL_20000000_HANDLE
#define UART1 __METAL_DT_SERIAL_20008000_HANDLE

#define DEBUG_HCA

#ifdef DEBUG_HCA
# define LPRINTF(_f_, _l_, _msg_, ...) \
    printf("%s[%d] " _msg_ "\n", _f_, _l_, ##__VA_ARGS__)
# define PRINTF(_msg_, ...) \
    LPRINTF(__func__, __LINE__, _msg_, ##__VA_ARGS__)
#else // DEBUG_HCA
# define LPRINTF(_f_, _l_, _msg_, ...)
# define PRINTF(_msg_, ...)
#endif

#ifndef DEBUG
#error Test GitHub Action
#endif

int main(void) {

    printf("Hello, World!\n");

#if 0 // __riscv_xlen == 32
    #error
    // secondary UART not yet defined on 64-bit platform
    const char str[] = "HW! direct\n";
    size_t len = strlen(str);
    for (unsigned int ix = 0; ix < len; ++ix) {
        metal_uart_putc(UART1, str[ix]);
    }

    printf("Hello, World! after\n");
    for(;;) {
        int c;
        metal_uart_getc(UART1, &c);
        if ( c != -1 ) {
            printf("h:%02x\n", c);
            if ( 'a' <= c && c <= 'z' ) {
                c -= 0x20;
            } else if ( 'A' <= c && c <= 'Z' ) {
                c += 0x20;
            }
            metal_uart_putc(UART1, c);
        } else {
            for(volatile int ix=0; ix<10000000; ix++) {}
        }
    }
#endif

    return 0;
}
