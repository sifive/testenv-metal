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

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

struct buf_desc {
    const uint8_t * bd_base;
    union {
        size_t bd_length;  /**< Size in bytes */
        size_t bd_count;   /**< Size in DMA block count */
    };
};

struct sha_desc {
    struct buf_desc sd_prolog;
    struct buf_desc sd_main;
    struct buf_desc sd_epilog;
};

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define DMA_BLOCK_SIZE     16u   // bytes
#define SHA512_BLOCK_SIZE  128u  // bytes
#define SHA512_LEN_SIZE    16u   // bytes

#define SHA256_BLOCKSIZE   64u   // bytes
#define SHA256_LEN_SIZE    8u    // bytes

#define HCA_BASE           (METAL_SIFIVE_HCA_0_BASE_ADDRESS)

#if 1
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
#else
static const char TEXT[] __attribute__((aligned(32)))="abc";
#endif

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a_) (sizeof((_a_))/sizeof((_a_)[0]))
#endif // ARRAY_SIZE

//-----------------------------------------------------------------------------
// Debug
//-----------------------------------------------------------------------------

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

#define DUMP_HEX(_msg_, _buf_, _len_) \
   _hca_hexdump(__func__, __LINE__, _msg_, _buf_, _len_);

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

#if __riscv_xlen >= 64
static uint64_t sha2[512u/(CHAR_BIT*sizeof(uint64_t))];
#else
static uint32_t sha2[512u/(CHAR_BIT*sizeof(uint32_t))];
#endif

static uint8_t trail_buf[2u*SHA512_BLOCK_SIZE] __attribute__((aligned(32)));
static uint8_t lead_buf[DMA_BLOCK_SIZE] __attribute__((aligned(8)));


//-----------------------------------------------------------------------------
// Implementation
//-----------------------------------------------------------------------------

static void
_hca_hexdump(const char * func, int line, const char * msg,
                    const uint8_t *buf, size_t size)
{
    static const char _hex[] = "0123456789ABCDEF";
    static char hexstr[512];
    for (unsigned int ix = 0 ; ix < size ; ix++) {
        hexstr[(ix * 2)] = _hex[(buf[ix] >> 4) & 0xf];
        hexstr[(ix * 2) + 1] = _hex[buf[ix] & 0xf];
    }
    hexstr[size * 2] = '\0';
    printf("%s[%d] %s (%zu): %s\n", func, line, msg, size, hexstr);
}

static inline void
_hca_updreg32(uint32_t reg, uint32_t value, size_t offset, uint32_t mask)
{
    uint32_t reg32;
    reg32 = METAL_REG32(HCA_BASE, reg);
    reg32 &= ~(mask << offset);
    reg32 |= ((value & mask) << offset);
    METAL_REG32(HCA_BASE, reg) = reg32;
}

static inline bool
_hca_sha_is_busy(void)
{
    uint32_t sha_cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_SHA_CR);
    return !! (sha_cr & (HCA_REGISTER_SHA_CR_BUSY_MASK <<
                         HCA_REGISTER_SHA_CR_BUSY_OFFSET));
}

static inline bool
_hca_dma_is_busy(void)
{
    uint32_t dma_cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_CR);
    return !! (dma_cr & (HCA_REGISTER_DMA_CR_BUSY_MASK <<
                         HCA_REGISTER_DMA_CR_BUSY_OFFSET));
}

static void
_hca_sha_get_hash(const uint8_t ** hash, size_t length)
{
    #if __riscv_xlen >= 64
    size_t size = length/sizeof(uint64_t);
    #else
    size_t size = length/sizeof(uint32_t);
    #endif
    for(unsigned int ix=0; ix<size; ix++) {
        sha2[size - 1u - ix] =
            #if __riscv_xlen >= 64
            __builtin_bswap64(METAL_REG64(HCA_BASE,
                              METAL_SIFIVE_HCA_HASH+ix*sizeof(uint64_t)));
            #else
            __builtin_bswap32(METAL_REG32(HCA_BASE,
                              METAL_SIFIVE_HCA_HASH+ix*sizeof(uint32_t)));
            #endif
    }
    *hash = (const uint8_t *)&sha2[0];
}

static void
_update_bit_len(uint8_t * eob, uint64_t length)
{
    const uint8_t *plen = (const uint8_t *)&length;
    unsigned int count = sizeof(uint64_t);

    while ( count-- ) {
        *--eob = *plen++;
    }
}

static int
_build_sha_desc(struct sha_desc * desc, const uint8_t * src, size_t length)
{
    // for now, do not manage prolog
    desc->sd_prolog.bd_base = NULL;
    desc->sd_prolog.bd_length = 0u;

    size_t trailing = length % DMA_BLOCK_SIZE;
    size_t payload_size = length - trailing;
    size_t skip = payload_size % SHA512_BLOCK_SIZE;

    desc->sd_main.bd_base = src;
    desc->sd_main.bd_count = length / DMA_BLOCK_SIZE;

    unsigned int pos = skip;
    memcpy(&trail_buf[pos], &src[payload_size], trailing);
    pos += trailing;
    trail_buf[pos++] = 0x80;

    size_t rem = SHA512_BLOCK_SIZE-pos;
    size_t buf_size = SHA512_BLOCK_SIZE-skip;

    if ( rem < SHA512_LEN_SIZE ) {
        if ( rem ) {
            memset(&trail_buf[pos], 0, rem);
        }
        pos += rem;
        rem = SHA512_BLOCK_SIZE;
        buf_size += SHA512_BLOCK_SIZE;
    }
    memset(&trail_buf[pos], 0, rem);
    pos += rem;
    _update_bit_len(&trail_buf[pos], length*CHAR_BIT);

    desc->sd_epilog.bd_base = &trail_buf[skip];
    desc->sd_epilog.bd_count = buf_size / DMA_BLOCK_SIZE;

    if ( buf_size % DMA_BLOCK_SIZE ) {
        PRINTF("ERROR !! %zu", buf_size);
        return -1;
    }
    // DUMP_HEX("Buffer:", desc->sd_epilog.bd_base, buf_size);

    return 0;
}

int run(void) {
    uint32_t reg;

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_HCA_REV);
    PRINTF("HCA rev: %08x", reg);
    if ( ! reg ) {
        return -1;
    }

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_SHA_REV);
    PRINTF("SHA rev: %08x", reg);
    if ( ! reg ) {
        return -1;
    }

    // FIFO mode: SHA
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_IFIFOTGT_OFFSET,
                  HCA_REGISTER_CR_IFIFOTGT_MASK);

    // IRQ: not on Crypto done
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
                  HCA_REGISTER_CR_CRYPTODIE_OFFSET,
                  HCA_REGISTER_CR_CRYPTODIE_MASK);
    // IRQ: not on output FIFO not empty
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
                  HCA_REGISTER_CR_OFIFOIE_OFFSET,
                  HCA_REGISTER_CR_OFIFOIE_MASK);
    // IRQ: not on DMA done
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
                  HCA_REGISTER_CR_DMADIE_OFFSET,
                  HCA_REGISTER_CR_DMADIE_MASK);

    // SHA mode: SHA2-512
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, 0x3,
                  HCA_REGISTER_SHA_CR_MODE_OFFSET,
                  HCA_REGISTER_SHA_CR_MODE_MASK);

    struct sha_desc desc;
    // skip trailing NUL char
    if ( _build_sha_desc(&desc, (const uint8_t *)TEXT, sizeof(TEXT)-1u) ){
        return -1;
    }

    if ( _hca_sha_is_busy() ) {
        return -1;
    }

    if ( _hca_dma_is_busy() ) {
        return -1;
    }

    PRINTF("SHA start");

    // SHA start
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, 1,
                  HCA_REGISTER_SHA_CR_INIT_OFFSET,
                  HCA_REGISTER_SHA_CR_INIT_MASK);

    PRINTF("DMA data start");

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =
        (uintptr_t)desc.sd_main.bd_base;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = 0u;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = desc.sd_main.bd_count;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    while( _hca_dma_is_busy() ) {
        // busy loop
    }

    PRINTF("DMA data done");

    while ( _hca_sha_is_busy() ) {
        // busy loop
    }

    PRINTF("SHA finish done");

    // DMA source & size
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =
        (uintptr_t)desc.sd_epilog.bd_base;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = desc.sd_epilog.bd_count;

    PRINTF("DMA finish start");

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                   HCA_REGISTER_DMA_CR_START_OFFSET,
                   HCA_REGISTER_DMA_CR_START_MASK);

    while(_hca_dma_is_busy()) {
        // busy loop
    }

    PRINTF("DMA finish done");

    while ( _hca_sha_is_busy() ) {
        // busy loop
    }

    PRINTF("SHA finish done");
    const uint8_t * hash;
    _hca_sha_get_hash(&hash, 512u/CHAR_BIT);
    DUMP_HEX("SHA512:", hash, 512u/CHAR_BIT);

    for(;;) {
        __asm__ volatile ("wfi");
    }
    PRINTF("END");

    return 0;
}


int main(void) {

    run();

    return 0;
}
