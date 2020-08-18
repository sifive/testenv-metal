#ifndef DMA_TEST_H
#define DMA_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "api/hardware/v0.5/random/hca_trng.h"
#include "api/hardware/v0.5/sifive_hca-0.5.x.h"
#include "api/hardware/hca_utils.h"
#include "api/hardware/hca_macro.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define DEBUG_HCA

#define HCA_BASE             (METAL_SIFIVE_HCA_0_BASE_ADDRESS)
#define HCA_ASD_IRQ_CHANNEL  23u
#define HCA_TRNG_IRQ_CHANNEL 24u

#define TIME_BASE            32768u // cannot rely on buggy metal API
#define HEART_BEAT_FREQUENCY 32u
#define HEART_BEAT_TIME      ((TIME_BASE)/(HEART_BEAT_FREQUENCY))

#define PAGE_SIZE            4096  // bytes
#define DMA_ALIGNMENT        32u   // bytes
#define DMA_BLOCK_SIZE       16u   // bytes

#define AES_BLOCK_SIZE       16u   // bytes

#define SHA512_BLOCK_SIZE    128u  // bytes
#define SHA512_LEN_SIZE      16u   // bytes
#define SHA256_BLOCKSIZE     64u   // bytes
#define SHA256_LEN_SIZE      8u    // bytes

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

struct worker {
    volatile size_t wk_crypto_count; /**< Count of crypto block IRQs */
    volatile size_t wk_dma_count;    /**< Count of DMA block IRQs */
    volatile size_t wk_crypto_total;
    volatile size_t wk_dma_total;
};

struct buf_desc {
    union {
        const uint8_t * bd_base;
        uint8_t * bd_dest;
    };
    union {
        size_t bd_length;  /**< Size in bytes */
        size_t bd_count;   /**< Size in DMA block count */
    };
};

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a_) (sizeof((_a_))/sizeof((_a_)[0]))
#endif // ARRAY_SIZE

#define ALIGN(_a_) __attribute__((aligned((_a_))))

#ifndef METAL_REG16
#define METAL_REG16(base, offset)                                              \
    (__METAL_ACCESS_ONCE((uint16_t *)((base) + (offset))))
#endif

#ifndef METAL_REG8
#define METAL_REG8(base, offset)                                              \
    (__METAL_ACCESS_ONCE((uint8_t *)((base) + (offset))))
#endif

#ifndef MIN
#define MIN(_a_, _b_) ((_a_) < (_b_) ? (_a_) : (_b_))
#endif // MIN

#ifndef MAX
#define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))
#endif // MAX

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
#define HCA_CR_OFIFO_EMPTY_BIT \
    (HCA_REGISTER_CR_OFIFOEMPTY_MASK << HCA_REGISTER_CR_OFIFOEMPTY_OFFSET)
#define HCA_CR_IFIFO_FULL_BIT \
    (HCA_REGISTER_CR_IFIFOFULL_MASK << HCA_REGISTER_CR_IFIFOFULL_OFFSET)
#define HCA_CR_OFIFO_FULL_BIT \
    (HCA_REGISTER_CR_OFIFOFULL_MASK << HCA_REGISTER_CR_OFIFOFULL_OFFSET)

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
   hca_hexdump(__func__, __LINE__, _msg_, _buf_, _len_);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

extern uint8_t ALIGN(DMA_ALIGNMENT) dma_long_buf[4*PAGE_SIZE];

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

void
hca_hexdump(const char * func, int line, const char * msg,
            const uint8_t *buf, size_t size);

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
_hca_aes_is_busy(void)
{
    uint32_t sha_cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_CR);
    return !! (sha_cr & (HCA_REGISTER_AES_CR_BUSY_MASK <<
                         HCA_REGISTER_AES_CR_BUSY_OFFSET));
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

static inline void
_hca_set_aes_key128(const uint8_t * key)
{
    const uint32_t * dwkey = (const uint32_t *)key;

    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_KEY+0x1cu) =
        __builtin_bswap32(dwkey[0u]);
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_KEY+0x18u) =
        __builtin_bswap32(dwkey[1u]);
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_KEY+0x14u) =
        __builtin_bswap32(dwkey[2u]);
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_KEY+0x10u) =
        __builtin_bswap32(dwkey[3u]);
}

static inline void
_hca_set_aes_iv96(const uint8_t * iv)
{
    const uint32_t * dwiv = (const uint32_t *)iv;

    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_INITV+0x0cu) =
        __builtin_bswap32(dwiv[0u]);
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_INITV+0x08u) =
        __builtin_bswap32(dwiv[1u]);
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_INITV+0x04u) =
        __builtin_bswap32(dwiv[2u]);
}


#endif // DMA_TEST_H