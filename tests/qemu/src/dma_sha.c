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
#include "unity_fixture.h"

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
    struct buf_desc sd_prolog; /**< Send w/o DMA: non-aligned start bytes */
    struct buf_desc sd_main;   /**< Send w/ DMA: aligned payload */
    struct buf_desc sd_finish; /**< Send w/ DMA: remainging paylod + padding */
    struct buf_desc sd_epilog; /**< Send w/o DMA: non-aligned end bytes */
};

struct worker {
    volatile size_t wk_crypto_count; /**< Count of crypto block IRQs */
    volatile size_t wk_dma_count;    /**< Count of DMA block IRQs */
    volatile size_t wk_crypto_total;
    volatile size_t wk_dma_total;
};

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a_) (sizeof((_a_))/sizeof((_a_)[0]))
#endif // ARRAY_SIZE

#define ALIGN(_a_) __attribute__((aligned((_a_))))

#ifndef METAL_REG16
#define METAL_REG16(base, offset) \
    (__METAL_ACCESS_ONCE((uint16_t *)((base) + (offset))))
#endif

#ifndef METAL_REG8
#define METAL_REG8(base, offset) \
    (__METAL_ACCESS_ONCE((uint8_t *)((base) + (offset))))
#endif

#define HCA_DMA_CR_ERROR_BITS                    \
    ((HCA_REGISTER_DMA_CR_RDALIGNERR_MASK <<     \
        HCA_REGISTER_DMA_CR_RDALIGNERR_OFFSET) | \
     (HCA_REGISTER_DMA_CR_WRALIGNERR_MASK <<     \
        HCA_REGISTER_DMA_CR_WRALIGNERR_OFFSET) | \
     (HCA_REGISTER_DMA_CR_RESPERR_MASK <<        \
        HCA_REGISTER_DMA_CR_RESPERR_OFFSET) |    \
     (HCA_REGISTER_DMA_CR_LEGALERR_MASK <<       \
        HCA_REGISTER_DMA_CR_LEGALERR_OFFSET))

#define HCA_DMA_CR_RD_ERROR_BIT                  \
    (HCA_REGISTER_DMA_CR_RDALIGNERR_MASK <<      \
        HCA_REGISTER_DMA_CR_RDALIGNERR_OFFSET)

#define HCA_CR_IFIFO_EMPTY_BIT \
    (HCA_REGISTER_CR_IFIFOEMPTY_MASK << HCA_REGISTER_CR_IFIFOEMPTY_OFFSET)
#define HCA_CR_IFIFO_FULL_BIT \
    (HCA_REGISTER_CR_IFIFOFULL_MASK << HCA_REGISTER_CR_IFIFOFULL_OFFSET)

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define HCA_BASE             (METAL_SIFIVE_HCA_0_BASE_ADDRESS)
#define HCA_ASD_IRQ_CHANNEL  23u

#define TIME_BASE            32768u // cannot rely on buggy metal API
#define HEART_BEAT_FREQUENCY 32u
#define HEART_BEAT_TIME      ((TIME_BASE)/(HEART_BEAT_FREQUENCY))

#define PAGE_SIZE            4096  // bytes
#define DMA_ALIGNMENT        32u   // bytes
#define DMA_BLOCK_SIZE       16u   // bytes
#define SHA512_BLOCK_SIZE    128u  // bytes
#define SHA512_LEN_SIZE      16u   // bytes

#define SHA256_BLOCKSIZE     64u   // bytes
#define SHA256_LEN_SIZE      8u    // bytes

static const char _TEXT[] __attribute__((aligned(DMA_ALIGNMENT))) =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Mauris pellentesque "
"auctor purus quis euismod. Duis laoreet finibus varius. Aenean egestas massa "
"ac nunc placerat, quis accumsan arcu fermentum. Curabitur lectus metus, "
"suscipit in est sed, elementum imperdiet sapien. Morbi feugiat non sem ac "
"auctor. Suspendisse ullamcorper iaculis congue. Nullam vitae leo sed odio "
"semper ornare. Aenean bibendum eget orci sed malesuada. Praesent placerat "
"sit amet justo euismod suscipit. Pellentesque ut placerat libero. Etiam in "
"velit tortor. Ut id arcu sit amet odio malesuada mollis non id velit. Nullam "
"id congue odio. Vivamus tincidunt arcu nisi, ut eleifend eros aliquam "
"blandit.";

static const uint8_t _TEXT_HASH[] = {
    0x5E, 0x29, 0xD6, 0x26, 0x94, 0x4B, 0xAB, 0xC1, 0xB5, 0xE4, 0x27, 0x3E,
    0xC0, 0xF0, 0x0D, 0x32, 0x98, 0x7C, 0xFB, 0xA8, 0x91, 0x60, 0xA3, 0xB4,
    0xE5, 0xFE, 0x37, 0xEB, 0x30, 0xF4, 0x8D, 0x69, 0xAF, 0x66, 0xF2, 0xFA,
    0xB4, 0x2F, 0xF0, 0x7D, 0xE4, 0xC7, 0x8C, 0xEF, 0xB0, 0xBF, 0x61, 0x06,
    0x7B, 0xE2, 0x4A, 0x72, 0x8F, 0x95, 0x15, 0xBF, 0xCA, 0xFD, 0x20, 0xC0,
    0x9B, 0xD9, 0x4F, 0xC6
};

static const uint8_t _LONG_BUF_HASH[] = {
    0x0A, 0x98, 0xCF, 0xDD, 0xB0, 0x8E, 0x08, 0x50, 0xC9, 0x20, 0xB2,
    0x96, 0x70, 0x04, 0x42, 0x5E, 0x2B, 0x2E, 0x8F, 0xA9, 0x4A, 0xF5,
    0xCE, 0x8E, 0xBD, 0x0B, 0x2C, 0xA1, 0x59, 0x43, 0xF4, 0x25, 0x27,
    0x24, 0x53, 0xA2, 0x48, 0x41, 0x46, 0xB6, 0x83, 0x2A, 0x01, 0x95,
    0x70, 0xF5, 0x27, 0xE6, 0xAA, 0xAC, 0xF9, 0x0B, 0xE5, 0x79, 0x06,
    0x5B, 0x9F, 0xF3, 0xA7, 0x6E, 0xA0, 0xFE, 0x10, 0x5B
};

//-----------------------------------------------------------------------------
// Debug
//-----------------------------------------------------------------------------

#define DEBUG_HCA
#undef SHOW_STEP

#ifdef DEBUG_HCA
# define LPRINTF(_f_, _l_, _msg_, ...) \
    printf("%s[%d] " _msg_ "\n", _f_, _l_, ##__VA_ARGS__)
# define PRINTF(_msg_, ...) \
    LPRINTF(__func__, __LINE__, _msg_, ##__VA_ARGS__)
#else // DEBUG_HCA
# define LPRINTF(_f_, _l_, _msg_, ...)
# define PRINTF(_msg_, ...)
#endif

#define HEX_LINE_LEN  64u

#define DUMP_HEX(_msg_, _buf_, _len_) \
   _hca_hexdump(__func__, __LINE__, _msg_, _buf_, _len_);

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

static struct worker _work;
static uint8_t _sha2_buf[512u/CHAR_BIT] ALIGN(sizeof(uint64_t));
static uint8_t _src_buf[sizeof(_TEXT)+DMA_ALIGNMENT] ALIGN(DMA_ALIGNMENT);
static uint8_t _trail_buf[2u*SHA512_BLOCK_SIZE] ALIGN(DMA_ALIGNMENT);
static uint8_t _long_buf[4u*PAGE_SIZE+DMA_ALIGNMENT] ALIGN(DMA_ALIGNMENT);

//-----------------------------------------------------------------------------
// Inline helpers
//-----------------------------------------------------------------------------

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

static inline bool
_hca_crypto_is_irq(void)
{
    uint32_t sha_cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    return !! (sha_cr & (HCA_REGISTER_CR_CRYPTODIS_MASK <<
                         HCA_REGISTER_CR_CRYPTODIS_OFFSET));
}

static inline bool
_hca_dma_is_irq(void)
{
    uint32_t dma_cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    return !! (dma_cr & (HCA_REGISTER_CR_DMADIS_MASK <<
                         HCA_REGISTER_CR_DMADIS_OFFSET));
}

static inline void
_hca_crypto_clear_irq(void)
{
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_CRYPTODIS_OFFSET,
                  HCA_REGISTER_CR_CRYPTODIS_MASK);
}

static inline void
_hca_dma_clear_irq(void)
{
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                   HCA_REGISTER_CR_DMADIS_OFFSET,
                   HCA_REGISTER_CR_DMADIS_MASK);
}

//-----------------------------------------------------------------------------
// Debug helpers
//-----------------------------------------------------------------------------

static void
_hca_hexdump(const char * func, int line, const char * msg,
                    const uint8_t *buf, size_t size)
{
    static const char _hex[] = "0123456789ABCDEF";
    static char hexstr[HEX_LINE_LEN*2u+1u];
    const uint8_t * end = buf+size;
    while ( buf < end ) {
        unsigned int ix = 0;
        while ( (ix < HEX_LINE_LEN) && (buf < end) ) {
            hexstr[(ix * 2)] = _hex[(*buf >> 4) & 0xf];
            hexstr[(ix * 2) + 1] = _hex[*buf & 0xf];
            buf++; ix++;
        }
        hexstr[2*HEX_LINE_LEN] = '\0';
        printf("%s[%d] %s (%zu): %s\n", func, line, msg, size, hexstr);
    }
}

//-----------------------------------------------------------------------------
// DMA SHA test implementation
//-----------------------------------------------------------------------------

static void
_hca_sha_get_hash(uint8_t * hash, size_t length)
{
    // hash should be aligned, not checked here
    #if __riscv_xlen >= 64
    size_t size = length/sizeof(uint64_t);
    uint64_t * ptr = (uint64_t *)hash;
    #else
    size_t size = length/sizeof(uint32_t);
    uint32_t * ptr = (uint32_t *)hash;
    #endif
    for(unsigned int ix=0; ix<size; ix++) {
        ptr[size - 1u - ix] =
            #if __riscv_xlen >= 64
            __builtin_bswap64(METAL_REG64(HCA_BASE,
                              METAL_SIFIVE_HCA_HASH+ix*sizeof(uint64_t)));
            #else
            __builtin_bswap32(METAL_REG32(HCA_BASE,
                              METAL_SIFIVE_HCA_HASH+ix*sizeof(uint32_t)));
            #endif
    }
}

static void
_sha_update_bit_len(uint8_t * eob, uint64_t length)
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
    size_t unaligned_size = (uintptr_t)src & (DMA_ALIGNMENT - 1u);
    size_t msg_size = length;
    if ( unaligned_size ) {
        desc->sd_prolog.bd_base = src;
        desc->sd_prolog.bd_length = DMA_ALIGNMENT - unaligned_size;
        src += desc->sd_prolog.bd_length;
        length -= desc->sd_prolog.bd_length;
    } else {
        desc->sd_prolog.bd_base = NULL;
        desc->sd_prolog.bd_length = 0u;
    }

    desc->sd_main.bd_base = src;
    desc->sd_main.bd_count = length / DMA_BLOCK_SIZE;
    size_t main_length = desc->sd_main.bd_count * DMA_BLOCK_SIZE;
    src += main_length;
    length -= main_length;

    // count of bytes to complete a SHA512_BLOCK_SIZE
    size_t to_end = SHA512_BLOCK_SIZE - msg_size%SHA512_BLOCK_SIZE;
    if ( to_end < SHA512_LEN_SIZE ) {
        to_end += SHA512_BLOCK_SIZE;
    }

    // PRINTF("To end %zu, rem len %zu", to_end, length);

    memcpy(_trail_buf, src, length);
    uint8_t * ptr = &_trail_buf[length];
    memset(ptr, 0, to_end);
    *ptr |= 0x80;
    _sha_update_bit_len(&ptr[to_end], msg_size*CHAR_BIT);

    length += to_end;

    desc->sd_finish.bd_base = _trail_buf;
    desc->sd_finish.bd_count = length / DMA_BLOCK_SIZE;

    ptr = &_trail_buf[desc->sd_finish.bd_count*DMA_BLOCK_SIZE];
    length -= desc->sd_finish.bd_count*DMA_BLOCK_SIZE;
    if ( length ) {
        desc->sd_epilog.bd_base = ptr;
        desc->sd_epilog.bd_length = length;
    } else {
        desc->sd_epilog.bd_base = NULL;
        desc->sd_epilog.bd_count = 0;
    }

    #ifdef SHOW_STEP
    PRINTF("Prolog: %p %zu", desc->sd_prolog.bd_base,
           desc->sd_prolog.bd_length);
    PRINTF("Main:   %p %zu [%zu]", desc->sd_main.bd_base,
        desc->sd_main.bd_count*DMA_BLOCK_SIZE, desc->sd_main.bd_count);
    PRINTF("Finish: %p %zu [%zu]", desc->sd_finish.bd_base,
        desc->sd_finish.bd_count*DMA_BLOCK_SIZE, desc->sd_finish.bd_count);
    PRINTF("Epilog: %p %zu", desc->sd_epilog.bd_base,
           desc->sd_epilog.bd_length);
    #endif

    return 0;
}

static void
_sha_push(const uint8_t * src, size_t length)
{
    const uint8_t * end = src+length;
    while ( src < end ) {
        #if __riscv_xlen >= 64
        if ( !(((uintptr_t)src) & (sizeof(uint64_t)-1u)) &&
                (length >= sizeof(uint64_t))) {
            //PRINTF("64 bit push");
            METAL_REG64(HCA_BASE, METAL_SIFIVE_HCA_FIFO_IN) =
                *(const uint64_t *)src;
            src += sizeof(uint64_t);
            continue;
        }
        #endif // __riscv_xlen >= 64
        if ( ! (((uintptr_t)src) & (sizeof(uint32_t)-1u)) &&
                (length >= sizeof(uint32_t))) {
            //PRINTF("32 bit push");
            METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_FIFO_IN) =
                *(const uint32_t *)src;
            src += sizeof(uint32_t);
            continue;
        }
        if ( ! (((uintptr_t)src) & (sizeof(uint16_t)-1u)) &&
                (length >= sizeof(uint16_t))) {
            //PRINTF("16 bit push");
            METAL_REG16(HCA_BASE, METAL_SIFIVE_HCA_FIFO_IN) =
                *(const uint16_t *)src;
            src += sizeof(uint16_t);
            continue;
        }
        if ( ! (((uintptr_t)src) & (sizeof(uint8_t)-1u)) ) {
            //PRINTF("8 bit push");
            METAL_REG8(HCA_BASE, METAL_SIFIVE_HCA_FIFO_IN) =
                *(const uint8_t *)src;
            src += sizeof(uint8_t);
            continue;
        }
    }
}

static void
_test_sha_dma_unaligned_poll(const uint8_t * buf, size_t buflen) {
    uint32_t reg;

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_HCA_REV);
    if ( ! reg ) {
        PRINTF("HCA rev: %08x", reg);
        TEST_FAIL_MESSAGE("HCA rev is nil");
    }

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_SHA_REV);
    if ( ! reg ) {
        PRINTF("SHA rev: %08x", reg);
        TEST_FAIL_MESSAGE("SHA rev is nil");
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

    if ( _hca_sha_is_busy() ) {
        TEST_FAIL_MESSAGE("SHA HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    // SHA start (don't care about the results, but the FIFO in should be
    // emptied)
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, 1,
                  HCA_REGISTER_SHA_CR_INIT_OFFSET,
                  HCA_REGISTER_SHA_CR_INIT_MASK);

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =(uintptr_t)buf;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = 0u;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = buflen/DMA_BLOCK_SIZE;

    bool exp_fail = !!(((uintptr_t)buf) & (DMA_ALIGNMENT - 1u));

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1u,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    while( _hca_dma_is_busy() ) {
        // busy loop
    }

    uint32_t dma_cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_CR);

    if ( ! exp_fail ) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            dma_cr & HCA_DMA_CR_ERROR_BITS, 0u, "Unexpected DMA error");
    } else {
        TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(
            dma_cr & HCA_DMA_CR_ERROR_BITS, 0u, "Unexpected DMA success");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            dma_cr & HCA_DMA_CR_ERROR_BITS, HCA_DMA_CR_RD_ERROR_BIT,
            "Wrong DMA error");
    }

    // be sure to leave the IFIFO empty, or other tests would fail
    uint32_t hca_cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_CR);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        hca_cr & HCA_CR_IFIFO_EMPTY_BIT, 0u, "FIFI not empty");
}

static void
_test_sha_dma_poll(const uint8_t * refh, const uint8_t * buf, size_t buflen) {
    uint32_t reg;

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_HCA_REV);
    if ( ! reg ) {
        PRINTF("HCA rev: %08x", reg);
        TEST_FAIL_MESSAGE("HCA rev is nil");
    }

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_SHA_REV);
    if ( ! reg ) {
        PRINTF("SHA rev: %08x", reg);
        TEST_FAIL_MESSAGE("SHA rev is nil");
    }

    // FIFO mode: SHA
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_IFIFOTGT_OFFSET,
                  HCA_REGISTER_CR_IFIFOTGT_MASK);

    // FIFO endianess: natural order
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_ENDIANNESS_OFFSET,
                  HCA_REGISTER_CR_ENDIANNESS_MASK);

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

    // sanity check
    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    TEST_ASSERT_EQUAL_MESSAGE(reg & HCA_CR_IFIFO_EMPTY_BIT,
                              HCA_CR_IFIFO_EMPTY_BIT,
                              "FIFO in is not empty");
    TEST_ASSERT_EQUAL_MESSAGE(reg & HCA_CR_IFIFO_FULL_BIT, 0u,
                              "FIFO in is full");

    // SHA mode: SHA2-512
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, 0x3,
                  HCA_REGISTER_SHA_CR_MODE_OFFSET,
                  HCA_REGISTER_SHA_CR_MODE_MASK);

    struct sha_desc desc;

    if ( _build_sha_desc(&desc, buf, buflen) ){
        TEST_FAIL_MESSAGE("Cannot build sequence descriptor");
    }

    if ( _hca_sha_is_busy() ) {
        TEST_FAIL_MESSAGE("SHA HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    // SHA start
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, 1,
                  HCA_REGISTER_SHA_CR_INIT_OFFSET,
                  HCA_REGISTER_SHA_CR_INIT_MASK);

    if ( desc.sd_prolog.bd_length ) {
        _sha_push(desc.sd_prolog.bd_base, desc.sd_prolog.bd_length);
    }

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =
        (uintptr_t)desc.sd_main.bd_base;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = 0u;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = desc.sd_main.bd_count;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    size_t dma_loop = 0;
    while( _hca_dma_is_busy() ) {
        // busy loop
        dma_loop += 1;
    }

    while ( _hca_sha_is_busy() ) {
        // busy loop
    }

    if ( buflen > 4096u ) {
        // whenever the buffer is greater than the VM chunk size, we expect
        // the guest code to be re-scheduled before the VM DMA completion
        TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(
            1000u, dma_loop, "VM may have freeze guest code execution");
    }

    // DMA source & size
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =
        (uintptr_t)desc.sd_finish.bd_base;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = desc.sd_finish.bd_count;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                   HCA_REGISTER_DMA_CR_START_OFFSET,
                   HCA_REGISTER_DMA_CR_START_MASK);

    while(_hca_dma_is_busy()) {
        // busy loop
    }

    while ( _hca_sha_is_busy() ) {
        // busy loop
    }

    if ( desc.sd_epilog.bd_length ) {
        _sha_push(desc.sd_epilog.bd_base, desc.sd_epilog.bd_length);
        while ( _hca_sha_is_busy() ) {
            // busy loop
        }
    } else {
    }

    _hca_sha_get_hash(_sha2_buf, 512u/CHAR_BIT);
    if ( refh ) {
        if ( memcmp(_sha2_buf, refh, 512u/CHAR_BIT) ) {
            DUMP_HEX("Invalid hash:", _sha2_buf, 512u/CHAR_BIT);
            DUMP_HEX("Ref:         ", refh, 512u/CHAR_BIT);
            TEST_FAIL_MESSAGE("Hash mismatch");
        }
    }
}

static void
_hca_irq_handler(int id, void * opaque)
{
    struct worker * work = (struct worker *)opaque;

    uint32_t cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);

    if ( cr & (HCA_REGISTER_CR_DMADIS_MASK <<
               HCA_REGISTER_CR_DMADIS_OFFSET) ) {
        work->wk_dma_count += 1u;
        work->wk_dma_total += 1u;
        //puts("^D\n");
    }

    if ( cr & (HCA_REGISTER_CR_CRYPTODIS_MASK <<
               HCA_REGISTER_CR_CRYPTODIS_OFFSET) ) {
        work->wk_crypto_count += 1u;
        work->wk_crypto_total += 1u;
        //puts("^S\n");
    }
}

static void
_timer_irq_handler(int id, void * opaque) {
    struct metal_cpu *cpu = (struct metal_cpu *)opaque;
    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu)+HEART_BEAT_TIME);
    // puts("^T\n");
}

static void
_hca_irq_init(struct worker * work)
{
    // Lets get the CPU and and its interrupt
    struct metal_cpu *cpu;
    cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    TEST_ASSERT_NOT_NULL_MESSAGE(cpu, "Cannot get CPU");

    struct metal_interrupt *cpu_intr;
    cpu_intr = metal_cpu_interrupt_controller(cpu);
    TEST_ASSERT_NOT_NULL_MESSAGE(cpu_intr, "Cannot get CPU controller");
    metal_interrupt_init(cpu_intr);

    struct metal_interrupt * plic;
    plic = metal_interrupt_get_controller(METAL_PLIC_CONTROLLER, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(plic, "Cannot get PLIC");
     metal_interrupt_init(plic);

    int rc;
    rc = metal_interrupt_register_handler(plic, HCA_ASD_IRQ_CHANNEL,
                                          &_hca_irq_handler, work);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot register IRQ handler");

    rc = metal_interrupt_enable(plic, HCA_ASD_IRQ_CHANNEL);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot enable IRQ");

    metal_interrupt_set_threshold(plic, 1);
    metal_interrupt_set_priority(plic, HCA_ASD_IRQ_CHANNEL, 2);

    // use a timer IRQ as an easier workaround for a time vulnarability
    // issue between WFI instruction and ISR. To avoid being stuck in WFI,
    // add an hearbeat.
    struct metal_interrupt *tmr_intr;
    tmr_intr = metal_cpu_timer_interrupt_controller(cpu);
    if ( !tmr_intr ) {
        return;
    }
    metal_interrupt_init(tmr_intr);

    int tmr_id;
    tmr_id = metal_cpu_timer_get_interrupt_id(cpu);
    rc = metal_interrupt_register_handler(tmr_intr, tmr_id, _timer_irq_handler,
                                          cpu);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot register IRQ handler");

    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu)+HEART_BEAT_TIME);
    metal_interrupt_enable(tmr_intr, tmr_id);
    metal_interrupt_enable(cpu_intr, 0);
}

static void
_hca_irq_fini(void)
{
    int rc;

    struct metal_interrupt * plic;
    plic = metal_interrupt_get_controller(METAL_PLIC_CONTROLLER, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(plic, "Cannot get PLIC");

    rc = metal_interrupt_disable(plic, HCA_ASD_IRQ_CHANNEL);
    TEST_ASSERT_FALSE_MESSAGE(rc, "Cannot diable IRQ");


    // clear interrupts
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
                  HCA_REGISTER_CR_CRYPTODIE_OFFSET,
                  HCA_REGISTER_CR_CRYPTODIE_MASK);
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
                  HCA_REGISTER_CR_OFIFOIE_OFFSET,
                  HCA_REGISTER_CR_OFIFOIE_MASK);
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
                  HCA_REGISTER_CR_DMADIE_OFFSET,
                  HCA_REGISTER_CR_DMADIE_MASK);
}

static void
_test_sha_dma_irq(const uint8_t * refh, const uint8_t * buf, size_t buflen,
                  struct worker * work) {
    uint32_t step = 0u;
    uint32_t reg;

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_HCA_REV);
    if ( ! reg ) {
        TEST_FAIL_MESSAGE("HCA rev is nil");
        TEST_FAIL();
    }

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_SHA_REV);
    if ( ! reg ) {
        PRINTF("SHA rev: %08x", reg);
        TEST_FAIL_MESSAGE("SHA rev is nil");
    }

    // FIFO mode: SHA
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_IFIFOTGT_OFFSET,
                  HCA_REGISTER_CR_IFIFOTGT_MASK);

    // IRQ: on Crypto done
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
                  HCA_REGISTER_CR_CRYPTODIE_OFFSET,
                  HCA_REGISTER_CR_CRYPTODIE_MASK);
    // IRQ: not on output FIFO not empty
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
                  HCA_REGISTER_CR_OFIFOIE_OFFSET,
                  HCA_REGISTER_CR_OFIFOIE_MASK);
    // IRQ: on DMA done
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_DMADIE_OFFSET,
                  HCA_REGISTER_CR_DMADIE_MASK);

    // SHA mode: SHA2-512
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, 0x3,
                  HCA_REGISTER_SHA_CR_MODE_OFFSET,
                  HCA_REGISTER_SHA_CR_MODE_MASK);

    // SHA does not expect a destination buffer
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = 0u;

    static struct sha_desc desc;

    if ( _build_sha_desc(&desc, buf, buflen) ){
        TEST_FAIL_MESSAGE("Cannot build sequence descriptor");
    }

    if ( _hca_sha_is_busy() ) {
        TEST_FAIL_MESSAGE("SHA HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    // DMA IRQ is used to get notified whenever the DMA is complete
    // However, using Crypto IRQ is mostly sugar, as it would be difficult
    // to use it along with DMA: it should raises once every SHA block.
    // It cannot be enabled right once the DMA complete IRQ is raised, as there
    // would be a vulnerability window (64 to 80 cycles betwen both IRQs) and
    // the last crypto IRQ could be missed.
    // Another way would be to count and compare the expected crypto IRQ count
    // but as SHA blocks and DMA blocks are not in sync, especially with
    // unaligned source, it should be tracked across prolog, main, finish and
    // epilog steps...
    // it is far easier, and robust, to poll for crypto block completion after
    // the last step (epilog)

    memset(work, 0, sizeof(*work));

    // SHA start
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, 1,
                  HCA_REGISTER_SHA_CR_INIT_OFFSET,
                  HCA_REGISTER_SHA_CR_INIT_MASK);

    if ( desc.sd_prolog.bd_length ) {
        #ifdef SHOW_STEP
        PRINTF("1. Prolog");
        #endif
        _sha_push(desc.sd_prolog.bd_base, desc.sd_prolog.bd_length);
        if ( work->wk_dma_count ) {
            TEST_FAIL_MESSAGE("Unexpected DMA IRQ");
        }
        step |= 1<<0;
    }

    if ( desc.sd_main.bd_count ) {
        #ifdef SHOW_STEP
        PRINTF("2. Main");
        #endif

        // DMA config
        METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =
            (uintptr_t)desc.sd_main.bd_base;
        METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) =
            desc.sd_main.bd_count;

        if ( work->wk_dma_count || _hca_dma_is_irq() ) {
            TEST_FAIL_MESSAGE("Unexpected DMA IRQ");
        }

        // DMA start
        _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                      HCA_REGISTER_DMA_CR_START_OFFSET,
                      HCA_REGISTER_DMA_CR_START_MASK);

        while( ! work->wk_dma_count ) {
            __asm__ volatile ("wfi");
        }
        _hca_dma_clear_irq();

        step |= 1<<1;
        work->wk_dma_count = 0u;
    }

    if ( desc.sd_finish.bd_count ) {
        #ifdef SHOW_STEP
        PRINTF("3. Finish");
        #endif

        // DMA source & size
        METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =
            (uintptr_t)desc.sd_finish.bd_base;
        METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) =
            desc.sd_finish.bd_count;

        if ( work->wk_dma_count || _hca_dma_is_irq() ) {
            TEST_FAIL_MESSAGE("Unexpected DMA IRQ");
        }

        // DMA start
        _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                       HCA_REGISTER_DMA_CR_START_OFFSET,
                       HCA_REGISTER_DMA_CR_START_MASK);

        while( ! work->wk_dma_count ) {
            __asm__ volatile ("wfi");
        }
        _hca_dma_clear_irq();

        work->wk_dma_count = 0u;
        step |= 1<<2;
    }

    if ( desc.sd_epilog.bd_length ) {
        #ifdef SHOW_STEP
        PRINTF("4. Epilog");
        #endif
        _sha_push(desc.sd_epilog.bd_base, desc.sd_epilog.bd_length);

        if ( work->wk_dma_count || _hca_dma_is_irq() ) {
            TEST_FAIL_MESSAGE("Unexpected DMA IRQ");
        }
        step |= 1<<3;
    }

    // wait for crypto block completion, using polling
    while ( _hca_sha_is_busy() ) {
        // busy loop
    }

    _hca_sha_get_hash(_sha2_buf, 512u/CHAR_BIT);

    if ( refh ) {
        if ( memcmp(_sha2_buf, refh, 512u/CHAR_BIT) ) {
            DUMP_HEX("Invalid hash:", _sha2_buf, 512u/CHAR_BIT);
            DUMP_HEX("Ref:         ", refh, 512u/CHAR_BIT);
            TEST_FAIL_MESSAGE("Hash mismatch");
        }
    }
}

//-----------------------------------------------------------------------------
// Unity tests
//-----------------------------------------------------------------------------

TEST_GROUP(dma_sha_poll);

TEST_SETUP(dma_sha_poll) {}

TEST_TEAR_DOWN(dma_sha_poll) {}

TEST(dma_sha_poll, unaligned)
{
    // note: error behaviour will DMA/IRQ is not defined in HCA documentation
    // it needs to be addressed somehow
    for (unsigned int ix=0; ix<DMA_ALIGNMENT; ix++) {
        _test_sha_dma_unaligned_poll((const uint8_t *)&_TEXT[ix],
                                     DMA_BLOCK_SIZE);
    }
}

TEST(dma_sha_poll, sha512)
{
    _test_sha_dma_poll(_TEXT_HASH, (const uint8_t *)_TEXT, sizeof(_TEXT)-1u);
    for (unsigned int ix=1; ix<DMA_ALIGNMENT; ix++) {
        memcpy(&_src_buf[ix], _TEXT, sizeof(_TEXT));
        _test_sha_dma_poll(_TEXT_HASH, &_src_buf[ix], sizeof(_TEXT)-1u);
    }
}

TEST(dma_sha_poll, sha512_long)
{
    for(unsigned int ix=0;
        ix<(sizeof(_long_buf)-DMA_ALIGNMENT)/sizeof(uint32_t); ix++) {
        ((uint32_t*) _long_buf)[ix] = ix;
    }
    //DUMP_HEX("longbuf", _long_buf, sizeof(_long_buf));
    uint8_t * ptr = _long_buf;
    for (unsigned int ix=0; ix<DMA_ALIGNMENT; ix++) {
        _test_sha_dma_poll(_LONG_BUF_HASH, ptr,
                           sizeof(_long_buf)-DMA_ALIGNMENT);
        memmove(ptr+1, ptr, sizeof(_long_buf)-DMA_ALIGNMENT);
        ptr += 1;
    }
}

TEST_GROUP_RUNNER(dma_sha_poll)
{
    RUN_TEST_CASE(dma_sha_poll, unaligned);
    RUN_TEST_CASE(dma_sha_poll, sha512);
    RUN_TEST_CASE(dma_sha_poll, sha512_long);
}

TEST_GROUP(dma_sha_irq);

TEST_SETUP(dma_sha_irq)
{
    _hca_irq_init(&_work);
}

TEST_TEAR_DOWN(dma_sha_irq)
{
    _hca_irq_fini();
}

TEST(dma_sha_irq, sha512)
{
    _test_sha_dma_irq(_TEXT_HASH, (const uint8_t *)_TEXT, sizeof(_TEXT)-1u,
                      &_work);
    for (unsigned int ix=1; ix<DMA_ALIGNMENT; ix++) {
        memcpy(&_src_buf[ix], _TEXT, sizeof(_TEXT));
        _test_sha_dma_irq(_TEXT_HASH, &_src_buf[ix], sizeof(_TEXT)-1u, &_work);
    }
}

TEST_GROUP_RUNNER(dma_sha_irq)
{
    RUN_TEST_CASE(dma_sha_irq, sha512);
}
