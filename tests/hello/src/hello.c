#include <string.h>
#include <stdio.h>
#include "metal/machine.h"
#include "metal/tty.h"

#define UART0 __METAL_DT_SERIAL_20000000_HANDLE
#define UART1 __METAL_DT_SERIAL_20008000_HANDLE

int main() {

    printf("Hello, World!\n");

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

    return 0;
}
