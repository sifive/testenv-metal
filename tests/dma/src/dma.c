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
    struct buf_desc sd_prolog; /**< Send w/o DMA: non-aligned start bytes */
    struct buf_desc sd_main;   /**< Send w/ DMA: aligned payload */
    struct buf_desc sd_finish; /**< Send w/ DMA: remainging paylod + padding */
    struct buf_desc sd_epilog; /**< Send w/o DMA: non-aligned end bytes */
};

struct worker {
    bool wk_sha;
    bool wk_dma;
};

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define HCA_ASD_IRQ_CHANNEL 23u

#define DMA_ALIGNMENT       32u
#define DMA_BLOCK_SIZE      16u   // bytes
#define SHA512_BLOCK_SIZE   128u  // bytes
#define SHA512_LEN_SIZE     16u   // bytes

#define SHA256_BLOCKSIZE    64u   // bytes
#define SHA256_LEN_SIZE     8u    // bytes

#define HCA_BASE            (METAL_SIFIVE_HCA_0_BASE_ADDRESS)

#if 1
static const char TEXT[] __attribute__((aligned(DMA_ALIGNMENT)))=
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
#else
static const char TEXT[] __attribute__((aligned(DMA_ALIGNMENT)))="abc";
#endif

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a_) (sizeof((_a_))/sizeof((_a_)[0]))
#endif // ARRAY_SIZE

#define ALIGN(_a_) __attribute__((aligned((_a_))))

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

static uint8_t src_buf[sizeof(TEXT)+DMA_ALIGNMENT] ALIGN(DMA_ALIGNMENT);
static uint8_t trail_buf[2u*SHA512_BLOCK_SIZE] ALIGN(DMA_ALIGNMENT);


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
    size_t unaligned_size = (uintptr_t)src & (DMA_ALIGNMENT - 1u);
    size_t msg_size = length;
    if ( unaligned_size ) {
        desc->sd_prolog.bd_base = src;
        desc->sd_prolog.bd_length = DMA_ALIGNMENT - unaligned_size;
        src += desc->sd_prolog.bd_length;
        length -= desc->sd_prolog.bd_length;
    } else {
        // for now, do not manage prolog
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

    memcpy(trail_buf, src, length);
    uint8_t * ptr = &trail_buf[length];
    memset(ptr, 0, to_end);
    *ptr |= 0x80;
    _update_bit_len(&ptr[to_end], msg_size*CHAR_BIT);

    length += to_end;

    desc->sd_finish.bd_base = trail_buf;
    desc->sd_finish.bd_count = length / DMA_BLOCK_SIZE;

    ptr = &trail_buf[desc->sd_finish.bd_count*DMA_BLOCK_SIZE];
    length -= desc->sd_finish.bd_count*DMA_BLOCK_SIZE;
    if ( length ) {
        desc->sd_epilog.bd_base = ptr;
        desc->sd_epilog.bd_length = length;
    } else {
        desc->sd_epilog.bd_base = NULL;
        desc->sd_epilog.bd_count = 0;
    }

    #if 0
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

static int
_test_sha_dma_poll(const uint8_t * buf, size_t buflen) {
    uint32_t reg;

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_HCA_REV);
    if ( ! reg ) {
        PRINTF("HCA rev: %08x", reg);
        return -1;
    }

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_SHA_REV);
    if ( ! reg ) {
        PRINTF("SHA rev: %08x", reg);
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
    if ( _build_sha_desc(&desc, buf, buflen) ){
        PRINTF("Error");
        return -1;
    }

    if ( _hca_sha_is_busy() ) {
        PRINTF("Error");
        return -1;
    }

    if ( _hca_dma_is_busy() ) {
        PRINTF("Error");
        return -1;
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

    while( _hca_dma_is_busy() ) {
        // busy loop
    }

    while ( _hca_sha_is_busy() ) {
        // busy loop
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
        PRINTF("SHA epilog");
        _sha_push(desc.sd_epilog.bd_base, desc.sd_epilog.bd_length);
        while ( _hca_sha_is_busy() ) {
            // busy loop
        }
    } else {
        PRINTF("No epilog");
    }

    const uint8_t * hash;
    _hca_sha_get_hash(&hash, 512u/CHAR_BIT);
    DUMP_HEX("SHA512:", hash, 512u/CHAR_BIT);

#if 0
    // expect no interrupt
    for(;;) {
        __asm__ volatile ("wfi");
    }
    PRINTF("END");
#endif

    return 0;
}

static void
_hca_irq_handler(int id, void * opaque)
{
    struct worker * work = (struct worker *)opaque;

    uint32_t cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);

    if ( cr & (HCA_REGISTER_CR_CRYPTODIS_MASK <<
               HCA_REGISTER_CR_CRYPTODIS_OFFSET) ) {
        work->wk_sha = false;
        // clear SHA done IRQ
        _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                     HCA_REGISTER_CR_CRYPTODIS_OFFSET,
                     HCA_REGISTER_CR_CRYPTODIS_MASK);
        PRINTF("^SHA");
    }

    if ( cr & (HCA_REGISTER_CR_DMADIS_MASK <<
               HCA_REGISTER_CR_DMADIS_OFFSET) ) {
        work->wk_dma = false;
        // clear DMA done IRQ
        _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                      HCA_REGISTER_CR_DMADIS_OFFSET,
                      HCA_REGISTER_CR_DMADIS_MASK);
        PRINTF("^DMA");
    }
}

static void
_hca_irq_init(struct worker * work)
{
    // Lets get the CPU and and its interrupt
    struct metal_cpu *cpu;
    cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    if ( ! cpu ) {
        PRINTF("Abort. CPU is null.");
        return;
    }

    struct metal_interrupt *cpu_intr;
    cpu_intr = metal_cpu_interrupt_controller(cpu);
    if ( ! cpu_intr ) {
        PRINTF("Abort. CPU interrupt controller is null.");
    }
    metal_interrupt_init(cpu_intr);

    struct metal_interrupt * plic;
    plic = metal_interrupt_get_controller(METAL_PLIC_CONTROLLER, 0);
    if ( ! plic ) {
        PRINTF("No PLIC?");
        return;
    }
    metal_interrupt_init(plic);

    int rc;
    rc = metal_interrupt_register_handler(plic, HCA_ASD_IRQ_CHANNEL,
                                          &_hca_irq_handler, work);
    if ( rc ) {
        PRINTF("Cannot register ASD handler");
        return;
    }

    rc = metal_interrupt_enable(plic, HCA_ASD_IRQ_CHANNEL);
    if ( rc ) {
        PRINTF("Cannot enable ASD handler");
        return;
    }

    metal_interrupt_set_threshold(plic, 1);
    metal_interrupt_set_priority(plic, HCA_ASD_IRQ_CHANNEL, 2);
    metal_interrupt_enable(cpu_intr, 0);

    // enable SHA done IRQ
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_CRYPTODIE_OFFSET,
                  HCA_REGISTER_CR_CRYPTODIE_MASK);
    // enable DMA done IRQ
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_DMADIE_OFFSET,
                  HCA_REGISTER_CR_DMADIE_MASK);

#if 0
    rc = metal_interrupt_disable(plic, HCA_TRNG_IRQ_CHANNEL);
    if ( rc ) {
        PRINTF("Cannot disable TRNG handler");
        return;
    }
    // clear interrupt
    METAL_REG32(scl.hca_base, METAL_SIFIVE_HCA_TRNG_DATA);
#endif
}

static int
_test_sha_dma_irq(const uint8_t * buf, size_t buflen, struct worker * work) {
    uint32_t reg;

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_HCA_REV);
    if ( ! reg ) {
        PRINTF("HCA rev: %08x", reg);
        return -1;
    }

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_SHA_REV);
    if ( ! reg ) {
        PRINTF("SHA rev: %08x", reg);
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
    if ( _build_sha_desc(&desc, buf, buflen) ){
        PRINTF("Error");
        return -1;
    }

    if ( _hca_sha_is_busy() ) {
        PRINTF("Error");
        return -1;
    }

    if ( _hca_dma_is_busy() ) {
        PRINTF("Error");
        return -1;
    }

    work->wk_sha = true;
    work->wk_dma = true;

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


    while( work->wk_sha & work->wk_dma ) {
        __asm__ volatile ("wfi");
    }

    // DMA source & size
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =
        (uintptr_t)desc.sd_finish.bd_base;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = desc.sd_finish.bd_count;

    work->wk_sha = true;
    work->wk_dma = true;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                   HCA_REGISTER_DMA_CR_START_OFFSET,
                   HCA_REGISTER_DMA_CR_START_MASK);

    while( work->wk_sha & work->wk_dma ) {
        __asm__ volatile ("wfi");
    }

    if ( desc.sd_epilog.bd_length ) {
        PRINTF("SHA epilog");
        work->wk_sha = true;

        _sha_push(desc.sd_epilog.bd_base, desc.sd_epilog.bd_length);
        while( work->wk_sha ) {
            __asm__ volatile ("wfi");
        }
    } else {
        PRINTF("No epilog");
    }

    const uint8_t * hash;
    _hca_sha_get_hash(&hash, 512u/CHAR_BIT);
    DUMP_HEX("SHA512:", hash, 512u/CHAR_BIT);

#if 0
    // expect no interrupt
    for(;;) {
        __asm__ volatile ("wfi");
    }
    PRINTF("END");
#endif

    return 0;
}

static void
run(void) {
    PRINTF("-- ALIGNED");
    #if 0
    _test_sha_dma_poll((const uint8_t *)TEXT, sizeof(TEXT)-1u);
    for (unsigned int ix=1; ix<DMA_ALIGNMENT; ix++) {
        PRINTF("-- UNALIGNED %u", ix);
        memcpy(&src_buf[ix], TEXT, sizeof(TEXT));
        _test_sha_dma_poll(&src_buf[ix], sizeof(TEXT)-1u);
    }
    #endif
    struct worker work;
    _hca_irq_init(&work);
    _test_sha_dma_irq((const uint8_t *)TEXT, sizeof(TEXT)-1u, &work);
}

int main(void) {

    run();

    return 0;
}
