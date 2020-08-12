#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "metal/machine.h"
#include "metal/tty.h"
#include "api/hardware/v0.5/random/hca_trng.h"
#include "api/hardware/v0.5/sifive_hca-0.5.x.h"
#include "api/hardware/hca_utils.h"
#include "api/hardware/hca_macro.h"

#define DMA_BLOCK_SIZE     16u   // bytes
#define SHA512_BLOCKSIZE   128u  // bytes
#define SHA512_LEN_SIZE    16u   // bytes

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

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a_) (sizeof((_a_))/sizeof((_a_)[0]))
#endif // ARRAY_SIZE

static const metal_scl_t scl = {
    .hca_base = METAL_SIFIVE_HCA_0_BASE_ADDRESS,
};

static const char TEXT[] __attribute__((aligned(32)))=
"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Mauris pellentesque "
"auctor purus quis euismod. Duis laoreet finibus varius. Aenean egestas massa "
"ac nunc placerat, quis accumsan arcu fermentum. Curabitur lectus metus, "
"suscipit in est sed, elementum imperdiet sapien. Morbi feugiat non sem ac "
"auctor. Suspendisse ullamcorper iaculis congue. Nullam vitae leo sed odio "
"semper ornare. Aenean bibendum eget orci sed malesuada. Praesent placerat "
"sit amet justo euismod suscipit. Pellentesque ut placerat libero. Etiam in "
"velit tortor. Ut id arcu sit amet odio malesuada mollis non id velit. Nullam "
"id congue odio. Vivamus tincidunt arcu nisi, ut eleifend eros aliquam "
"blandit. Pellentesque sed diam placerat erat pharetra scelerisque.";

#if __riscv_xlen >= 64
static uint64_t sha2[512u/(CHAR_BIT*sizeof(uint64_t))];
#else
static uint32_t sha2[512u/(CHAR_BIT*sizeof(uint32_t))];
#endif

static uint8_t buffer[2u*SHA512_BLOCKSIZE] __attribute__((aligned(32)));

static void
_sifive_hca_hexdump(const char * func, int line, const char * msg,
                    const uint8_t *buf, size_t size)
{
    static const char _hex[] = "0123456789ABCDEF";
    static char hexstr[128];
    for (unsigned int ix = 0 ; ix < size ; ix++) {
        hexstr[(ix * 2)] = _hex[(buf[ix] >> 4) & 0xf];
        hexstr[(ix * 2) + 1] = _hex[buf[ix] & 0xf];
    }
    hexstr[size * 2] = '\0';
    printf("%s[%d] %s (%zu): %s\n", func, line, msg, size, hexstr);
}

#define DUMP_HEX(_msg_, _buf_, _len_) \
   _sifive_hca_hexdump(__func__, __LINE__, _msg_, _buf_, _len_);

void run(void) {
    uint32_t reg;

    reg = METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_HCA_REV);
    PRINTF("HCA rev: %08x", reg);
    if ( ! reg ) {
        return;
    }

    reg = METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_SHA_REV);
    PRINTF("SHA rev: %08x", reg);
    if ( ! reg ) {
        return;
    }

    size_t size = sizeof(TEXT);
    size_t dma_size = size / DMA_BLOCK_SIZE;
    size_t trailing = size % DMA_BLOCK_SIZE;

    PRINTF("Text trailing: %zu", trailing);
    memcpy(&buffer[0], &TEXT[size], trailing);
    buffer[trailing++] = 0x80;
    size_t rem = SHA512_BLOCKSIZE-trailing;

    size_t buf_size = SHA512_BLOCKSIZE;
    if ( rem < SHA512_LEN_SIZE ) {
        if ( rem ) {
            memset(&buffer[trailing], 0, rem);
        }
        trailing += rem;
        rem = SHA512_BLOCKSIZE;
        buf_size += SHA512_BLOCKSIZE;
    }
    memset(&buffer[trailing], 0, rem);
    trailing += rem - SHA512_LEN_SIZE;

    // DUMP_HEX("Buffer:", buffer, buf_size);

    PRINTF("DMA src:  %p", TEXT);
    PRINTF("DMA size: %zu -> %zu", size, dma_size);

    // FIFO mode: SHA
    hca_setfield32(&scl, METAL_SIFIVE_HCA_CR, 1,
                   HCA_REGISTER_CR_IFIFOTGT_OFFSET,
                   HCA_REGISTER_CR_IFIFOTGT_MASK);

    // IRQ: not on Crypto done
    hca_setfield32(&scl, METAL_SIFIVE_HCA_CR, 0,
                   HCA_REGISTER_CR_CRYPTODIE_OFFSET,
                   HCA_REGISTER_CR_CRYPTODIE_MASK);
    // IRQ: not on output FIFO not empty
    hca_setfield32(&scl, METAL_SIFIVE_HCA_CR, 0,
                   HCA_REGISTER_CR_OFIFOIE_OFFSET,
                   HCA_REGISTER_CR_OFIFOIE_MASK);
    // IRQ: not on DMA done
    hca_setfield32(&scl, METAL_SIFIVE_HCA_CR, 0,
                   HCA_REGISTER_CR_DMADIE_OFFSET,
                   HCA_REGISTER_CR_DMADIE_MASK);

    // SHA mode: SHA2-512
    hca_setfield32(&scl, METAL_SIFIVE_HCA_SHA_CR, 0x3,
                   HCA_REGISTER_SHA_CR_MODE_OFFSET,
                   HCA_REGISTER_SHA_CR_MODE_MASK);

    PRINTF("SHA start");

    // SHA start
    hca_setfield32(&scl, METAL_SIFIVE_HCA_SHA_CR, 1,
                   HCA_REGISTER_SHA_CR_INIT_OFFSET,
                   HCA_REGISTER_SHA_CR_INIT_MASK);

    PRINTF("DMA data start");

    // DMA source
    METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_DMA_SRC) = (uintptr_t)TEXT;

    // DMA destination (none)
    METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_DMA_DEST) = 0;

    // DMA size
    METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_DMA_LEN) = dma_size;

    // DMA start
    hca_setfield32(&scl, METAL_SIFIVE_HCA_DMA_CR, 1,
                   HCA_REGISTER_DMA_CR_START_OFFSET,
                   HCA_REGISTER_DMA_CR_START_MASK);

    for(;;) {
        // Poll on DMA busy
        uint32_t dma_cr = METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_DMA_CR);
        if ( ! (dma_cr & (HCA_REGISTER_DMA_CR_BUSY_MASK <<
                          HCA_REGISTER_DMA_CR_BUSY_OFFSET)) ) {
            break;
        }
    }

    PRINTF("DMA data done");

    for(;;) {
        // Poll on SHA busy
        uint32_t sha_cr = METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_SHA_CR);
        if ( ! (sha_cr & (HCA_REGISTER_SHA_CR_BUSY_MASK <<
                          HCA_REGISTER_SHA_CR_BUSY_OFFSET)) ) {
            break;
        }
    }

    PRINTF("SHA finish done");

    dma_size = buf_size / DMA_BLOCK_SIZE;
    PRINTF("DMA size: %zu -> %zu", buf_size, dma_size);

    // DMA source
    METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_DMA_SRC) = (uintptr_t)buffer;

    // DMA size
    METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_DMA_LEN) = dma_size;


    PRINTF("DMA finish start");

    // DMA start
    hca_setfield32(&scl, METAL_SIFIVE_HCA_DMA_CR, 1,
                   HCA_REGISTER_DMA_CR_START_OFFSET,
                   HCA_REGISTER_DMA_CR_START_MASK);

    for(;;) {
        // Poll on DMA busy
        uint32_t dma_cr = METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_DMA_CR);
        if ( ! (dma_cr & (HCA_REGISTER_DMA_CR_BUSY_MASK <<
                          HCA_REGISTER_DMA_CR_BUSY_OFFSET)) ) {
            break;
        }
    }

    PRINTF("DMA finish done");

    for(;;) {
        // Poll on SHA busy
        uint32_t sha_cr = METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_SHA_CR);
        if ( ! (sha_cr & (HCA_REGISTER_SHA_CR_BUSY_MASK <<
                          HCA_REGISTER_SHA_CR_BUSY_OFFSET)) ) {
            break;
        }
    }

    PRINTF("SHA finish done");
    for(unsigned int ix=0; ix<ARRAY_SIZE(sha2); ix++) {
#if __riscv_xlen >= 64
        sha2[ix] = METAL_REG64(scl.hca_base,
                               METAL_SIFIVE_HCA_HASH+ix*sizeof(uint64_t));
#else
        sha2[ix] = METAL_REG32(scl.hca_base,
                               METAL_SIFIVE_HCA_HASH+ix*sizeof(uint32_t));
#endif
    }
    const uint8_t * hash = (const uint8_t *)&sha2[0];
    DUMP_HEX("SHA512:", hash, 512u/CHAR_BIT);

    for(;;) {
        __asm__ volatile ("wfi");
    }
    PRINTF("END");
}


int main(void) {

    run();

    return 0;
}
