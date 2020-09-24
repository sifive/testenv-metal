#include "metal/machine.h"
#include "unity_fixture.h"
#include "qemu.h"

//-----------------------------------------------------------------------------
// Variable
//-----------------------------------------------------------------------------

uint8_t ALIGN(DMA_ALIGNMENT) dma_long_buf[4*PAGE_SIZE];
qemu_hart_task_t _metal_exec_array[MAX_HARTS];

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

void
qemu_hexdump(const char * func, int line, const char * msg,
             const uint8_t *buf, size_t size)
{
    static const char _hex[] = "0123456789ABCDEF";
    static char hexstr[HEX_LINE_LEN*2u+1u];
    const uint8_t * end = buf+size;
    unsigned int pos = 0;
    while ( buf < end ) {
        unsigned int ix = 0;
        while ( (ix < HEX_LINE_LEN) && (buf < end) ) {
            hexstr[(ix * 2)] = _hex[(*buf >> 4) & 0xf];
            hexstr[(ix * 2) + 1] = _hex[*buf & 0xf];
            buf++; ix++;
        }
        hexstr[2*ix] = '\0';
        if ( func && line ) {
            printf("%s[%d] %s (%zu)[%04x]: %s\n",
                   func, line, msg, size, pos, hexstr);
        } else {
            printf("%s (%zu)[%04x]: %s\n", msg, size, pos, hexstr);
        }
        pos += HEX_LINE_LEN;
    }
}

void
qemu_register_hart_task(unsigned int hartid, qemu_hart_task_t task)
{
    if ( hartid < ARRAY_SIZE(_metal_exec_array) ) {
        _metal_exec_array[hartid] = task;
    }
}

//-----------------------------------------------------------------------------
// Unit test main
//-----------------------------------------------------------------------------

static void
_ut_run(void)
{
    UnityFixture.Verbose = 1;
    // UnityFixture.GroupFilter = "dma_sha256_poll";
    // UnityFixture.NameFilter = "short_msg1_64";

    // RUN_TEST_GROUP(time_irq);
    RUN_TEST_GROUP(trng);
    RUN_TEST_GROUP(dma_sha256_poll);
    RUN_TEST_GROUP(dma_sha256_irq);
    RUN_TEST_GROUP(dma_sha512_poll);
    RUN_TEST_GROUP(dma_sha512_irq);
    RUN_TEST_GROUP(dma_aes_ecb_poll);
    RUN_TEST_GROUP(dma_aes_ecb_irq);
    RUN_TEST_GROUP(dma_aes_gcm_poll);
    RUN_TEST_GROUP(dma_aes_gcm_irq);
}

int main(int argc, const char *argv[])
{
    extern uint32_t __stack_size;
    uint32_t stack_size = (uint32_t)(uintptr_t)(&__stack_size);
    if ( stack_size < 0x1000u ) {
        // cannot event use Unity framework as the stack would be corrupted
        // by any call to *printf
        puts("Stack size too small");
        exit(1);
    }

    return UnityMain(argc, argv, _ut_run);
}
