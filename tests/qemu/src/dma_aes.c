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
#define METAL_REG16(base, offset)                                              \
    (__METAL_ACCESS_ONCE((uint16_t *)((base) + (offset))))
#endif

#ifndef METAL_REG8
#define METAL_REG8(base, offset)                                              \
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
#define HCA_CR_OFIFO_EMPTY_BIT \
    (HCA_REGISTER_CR_OFIFOEMPTY_MASK << HCA_REGISTER_CR_OFIFOEMPTY_OFFSET)
#define HCA_CR_IFIFO_FULL_BIT \
    (HCA_REGISTER_CR_IFIFOFULL_MASK << HCA_REGISTER_CR_IFIFOFULL_OFFSET)
#define HCA_CR_OFIFO_FULL_BIT \
    (HCA_REGISTER_CR_OFIFOFULL_MASK << HCA_REGISTER_CR_OFIFOFULL_OFFSET)

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

#define AES_BLOCK_SIZE       16u   // bytes

static const uint8_t _KEY_AES128[] = {
   0X2B, 0X7E, 0X15, 0X16, 0X28, 0XAE, 0XD2, 0XA6,
   0XAB, 0XF7, 0X15, 0X88, 0X09, 0XCF, 0X4F, 0X3C
};

static const uint8_t _PLAINTEXT_ECB[] ALIGN(DMA_ALIGNMENT) = {
    0X6B, 0XC1, 0XBE, 0XE2, 0X2E, 0X40, 0X9F, 0X96, 0XE9, 0X3D, 0X7E,
    0X11, 0X73, 0X93, 0X17, 0X2A, 0XAE, 0X2D, 0X8A, 0X57, 0X1E, 0X03,
    0XAC, 0X9C, 0X9E, 0XB7, 0X6F, 0XAC, 0X45, 0XAF, 0X8E, 0X51, 0X30,
    0XC8, 0X1C, 0X46, 0XA3, 0X5C, 0XE4, 0X11, 0XE5, 0XFB, 0XC1, 0X19,
    0X1A, 0X0A, 0X52, 0XEF, 0XF6, 0X9F, 0X24, 0X45, 0XDF, 0X4F, 0X9B,
    0X17, 0XAD, 0X2B, 0X41, 0X7B, 0XE6, 0X6C, 0X37, 0X10
};

static const uint8_t _CIPHERTEXT_ECB[] ALIGN(DMA_ALIGNMENT) = {
    0X3A, 0XD7, 0X7B, 0XB4, 0X0D, 0X7A, 0X36, 0X60, 0XA8, 0X9E, 0XCA,
    0XF3, 0X24, 0X66, 0XEF, 0X97, 0XF5, 0XD3, 0XD5, 0X85, 0X03, 0XB9,
    0X69, 0X9D, 0XE7, 0X85, 0X89, 0X5A, 0X96, 0XFD, 0XBA, 0XAF, 0X43,
    0XB1, 0XCD, 0X7F, 0X59, 0X8E, 0XCE, 0X23, 0X88, 0X1B, 0X00, 0XE3,
    0XED, 0X03, 0X06, 0X88, 0X7B, 0X0C, 0X78, 0X5E, 0X27, 0XE8, 0XAD,
    0X3F, 0X82, 0X23, 0X20, 0X71, 0X04, 0X72, 0X5D, 0XD4
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
static uint8_t _dst_buf[sizeof(_PLAINTEXT_ECB)] ALIGN(DMA_ALIGNMENT);
static uint8_t _long_buf[4u*PAGE_SIZE] ALIGN(DMA_ALIGNMENT);

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
// DMA AES test implementation
//-----------------------------------------------------------------------------


static void
_test_dma_poll(const uint8_t * ref_d, const uint8_t * ref_s, uint8_t * dst,
               uint8_t * src, size_t length, size_t repeat) {
    uint32_t reg;

    TEST_ASSERT_EQUAL_MESSAGE(((uintptr_t)src) & ((DMA_ALIGNMENT) - 1u), 0,
                              "Source is not aligned on a DMA boundary");
    TEST_ASSERT_EQUAL_MESSAGE(((uintptr_t)dst) & ((DMA_ALIGNMENT) - 1u), 0,
                              "Destination is not aligned on a DMA boundary");
    TEST_ASSERT_EQUAL_MESSAGE(length & (DMA_BLOCK_SIZE - 1u), 0,
                              "Length is not aligned on a DMA block size");

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_HCA_REV);
    if ( ! reg ) {
        PRINTF("HCA rev: %08x", reg);
        TEST_FAIL_MESSAGE("HCA rev is nil");
    }

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_REV);
    if ( ! reg ) {
        PRINTF("SHA rev: %08x", reg);
        TEST_FAIL_MESSAGE("AES rev is nil");
    }

    // FIFO mode: AES
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
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
    TEST_ASSERT_EQUAL_MESSAGE(reg &
                              (HCA_CR_IFIFO_EMPTY_BIT|HCA_CR_OFIFO_EMPTY_BIT),
                              HCA_CR_IFIFO_EMPTY_BIT|HCA_CR_OFIFO_EMPTY_BIT,
                              "FIFOs are not empty");
    TEST_ASSERT_EQUAL_MESSAGE(reg &
                              (HCA_CR_IFIFO_FULL_BIT|HCA_CR_OFIFO_FULL_BIT),
                              0u,
                              "FIFOs are full");

    // AES mode: ECB
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                  HCA_REGISTER_AES_CR_MODE_OFFSET,
                  HCA_REGISTER_AES_CR_MODE_MASK);
    // AES key size: 128 bits
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                  HCA_REGISTER_AES_CR_KEYSZ_OFFSET,
                  HCA_REGISTER_AES_CR_KEYSZ_MASK);
    // AES process: encryption
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                  HCA_REGISTER_AES_CR_PROCESS_OFFSET,
                  HCA_REGISTER_AES_CR_PROCESS_MASK);
    // AES init: no need
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                  HCA_REGISTER_AES_CR_INIT_OFFSET,
                  HCA_REGISTER_AES_CR_INIT_MASK);

    // AES key
    _hca_set_aes_key128(_KEY_AES128);

    if ( _hca_aes_is_busy() ) {
        TEST_FAIL_MESSAGE("AES HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    size_t chunk = length/repeat;

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) = (uintptr_t)src;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = (uintptr_t)dst;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = length/DMA_BLOCK_SIZE;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    size_t dma_loop = 0;
    while( _hca_dma_is_busy() ) {
        // busy loop
        dma_loop += 1;
    }

    while ( _hca_aes_is_busy() ) {
        // busy loop
    }

    if ( length > 4096u ) {
        // whenever the buffer is greater than the VM chunk size, we expect
        // the guest code to be re-scheduled before the VM DMA completion
        TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(
            1000u, dma_loop, "VM may have freeze guest code execution");
    }

    if ( ref_d ) {
        const uint8_t * ptr = dst;
        for (unsigned int ix=0; ix<repeat; ix++) {
            if ( memcmp(ptr, ref_d, chunk) ) {
                DUMP_HEX("Invalid AES:", ptr, chunk);
                DUMP_HEX("Ref:        ", ref_d, chunk);
                TEST_FAIL_MESSAGE("AES encryption mismatch");
            }
            ptr += chunk;
        }
    }

    // sanity check
    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    TEST_ASSERT_EQUAL_MESSAGE(reg &
                              (HCA_CR_IFIFO_EMPTY_BIT|HCA_CR_OFIFO_EMPTY_BIT),
                              HCA_CR_IFIFO_EMPTY_BIT|HCA_CR_OFIFO_EMPTY_BIT,
                              "FIFOs are not empty");
    TEST_ASSERT_EQUAL_MESSAGE(reg &
                              (HCA_CR_IFIFO_FULL_BIT|HCA_CR_OFIFO_FULL_BIT),
                              0u,
                              "FIFOs are full");

    // AES process: decryption
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 1u,
                  HCA_REGISTER_AES_CR_PROCESS_OFFSET,
                  HCA_REGISTER_AES_CR_PROCESS_MASK);

    // AES key
    _hca_set_aes_key128(_KEY_AES128);

    if ( _hca_aes_is_busy() ) {
        TEST_FAIL_MESSAGE("AES HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) = (uintptr_t)dst;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = (uintptr_t)src;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = length/DMA_BLOCK_SIZE;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    dma_loop = 0;
    while( _hca_dma_is_busy() ) {
        // busy loop
        dma_loop += 1;
    }

    while ( _hca_aes_is_busy() ) {
        // busy loop
    }

    if ( length > 4096u ) {
        // whenever the buffer is greater than the VM chunk size, we expect
        // the guest code to be re-scheduled before the VM DMA completion
        TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(
            1000u, dma_loop, "VM may have freeze guest code execution");
    }

    if ( ref_s ) {
        const uint8_t * ptr = src;
        for (unsigned int ix=0; ix<repeat; ix++) {
            if ( memcmp(ptr, ref_s, chunk) ) {
                DUMP_HEX("Invalid AES:", ptr, chunk);
                DUMP_HEX("Ref:        ", ref_s, chunk);
                TEST_FAIL_MESSAGE("AES decryption mismatch");
            }
            ptr += chunk;
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
_test_dma_irq(const uint8_t * ref_d, const uint8_t * ref_s, uint8_t * dst,
               uint8_t * src, size_t length, size_t repeat,
               struct worker * work) {
    uint32_t reg;

    TEST_ASSERT_EQUAL_MESSAGE(((uintptr_t)src) & ((DMA_ALIGNMENT) - 1u), 0,
                              "Source is not aligned on a DMA boundary");
    TEST_ASSERT_EQUAL_MESSAGE(((uintptr_t)dst) & ((DMA_ALIGNMENT) - 1u), 0,
                              "Destination is not aligned on a DMA boundary");
    TEST_ASSERT_EQUAL_MESSAGE(length & (DMA_BLOCK_SIZE - 1u), 0,
                              "Length is not aligned on a DMA block size");

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_HCA_REV);
    if ( ! reg ) {
        PRINTF("HCA rev: %08x", reg);
        TEST_FAIL_MESSAGE("HCA rev is nil");
    }

    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_REV);
    if ( ! reg ) {
        PRINTF("SHA rev: %08x", reg);
        TEST_FAIL_MESSAGE("AES rev is nil");
    }

    // FIFO mode: AES
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
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
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_DMADIE_OFFSET,
                  HCA_REGISTER_CR_DMADIE_MASK);

    // sanity check
    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    TEST_ASSERT_EQUAL_MESSAGE(reg &
                              (HCA_CR_IFIFO_EMPTY_BIT|HCA_CR_OFIFO_EMPTY_BIT),
                              HCA_CR_IFIFO_EMPTY_BIT|HCA_CR_OFIFO_EMPTY_BIT,
                              "FIFOs are not empty");
    TEST_ASSERT_EQUAL_MESSAGE(reg &
                              (HCA_CR_IFIFO_FULL_BIT|HCA_CR_OFIFO_FULL_BIT),
                              0u,
                              "FIFOs are full");

    // AES mode: ECB
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                  HCA_REGISTER_AES_CR_MODE_OFFSET,
                  HCA_REGISTER_AES_CR_MODE_MASK);
    // AES key size: 128 bits
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                  HCA_REGISTER_AES_CR_KEYSZ_OFFSET,
                  HCA_REGISTER_AES_CR_KEYSZ_MASK);
    // AES process: encryption
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                  HCA_REGISTER_AES_CR_PROCESS_OFFSET,
                  HCA_REGISTER_AES_CR_PROCESS_MASK);
    // AES init: no need
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                  HCA_REGISTER_AES_CR_INIT_OFFSET,
                  HCA_REGISTER_AES_CR_INIT_MASK);

    // AES key
    _hca_set_aes_key128(_KEY_AES128);

    if ( _hca_aes_is_busy() ) {
        TEST_FAIL_MESSAGE("AES HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    memset(work, 0, sizeof(*work));

    size_t chunk = length/repeat;

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) = (uintptr_t)src;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = (uintptr_t)dst;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = length/DMA_BLOCK_SIZE;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    while( ! work->wk_dma_count ) {
        __asm__ volatile ("wfi");
    }
    _hca_dma_clear_irq();

    while ( _hca_aes_is_busy() ) {
        // busy loop
    }

    if ( ref_d ) {
        const uint8_t * ptr = dst;
        for (unsigned int ix=0; ix<repeat; ix++) {
            if ( memcmp(ptr, ref_d, chunk) ) {
                DUMP_HEX("Invalid AES:", ptr, chunk);
                DUMP_HEX("Ref:        ", ref_d, chunk);
                TEST_FAIL_MESSAGE("AES encryption mismatch");
            }
            ptr += chunk;
        }
    }

    // sanity check
    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    TEST_ASSERT_EQUAL_MESSAGE(reg &
                              (HCA_CR_IFIFO_EMPTY_BIT|HCA_CR_OFIFO_EMPTY_BIT),
                              HCA_CR_IFIFO_EMPTY_BIT|HCA_CR_OFIFO_EMPTY_BIT,
                              "FIFOs are not empty");
    TEST_ASSERT_EQUAL_MESSAGE(reg &
                              (HCA_CR_IFIFO_FULL_BIT|HCA_CR_OFIFO_FULL_BIT),
                              0u,
                              "FIFOs are full");

    // AES process: decryption
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 1u,
                  HCA_REGISTER_AES_CR_PROCESS_OFFSET,
                  HCA_REGISTER_AES_CR_PROCESS_MASK);

    // AES key
    _hca_set_aes_key128(_KEY_AES128);

    if ( _hca_aes_is_busy() ) {
        TEST_FAIL_MESSAGE("AES HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    memset(work, 0, sizeof(*work));

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) = (uintptr_t)dst;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = (uintptr_t)src;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = length/DMA_BLOCK_SIZE;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    while( ! work->wk_dma_count ) {
        __asm__ volatile ("wfi");
    }
    _hca_dma_clear_irq();

    while ( _hca_aes_is_busy() ) {
        // busy loop
    }

    if ( ref_s ) {
        const uint8_t * ptr = src;
        for (unsigned int ix=0; ix<repeat; ix++) {
            if ( memcmp(ptr, ref_s, chunk) ) {
                DUMP_HEX("Invalid AES:", ptr, chunk);
                DUMP_HEX("Ref:        ", ref_s, chunk);
                TEST_FAIL_MESSAGE("AES decryption mismatch");
            }
            ptr += chunk;
        }
    }
}

//-----------------------------------------------------------------------------
// Unity tests
//-----------------------------------------------------------------------------

TEST_GROUP(dma_aes_poll);

TEST_SETUP(dma_aes_poll) {}

TEST_TEAR_DOWN(dma_aes_poll) {}

TEST(dma_aes_poll, ecb_short)
{
    memcpy(_long_buf, _PLAINTEXT_ECB, sizeof(_PLAINTEXT_ECB));
    _test_dma_poll(_CIPHERTEXT_ECB, _PLAINTEXT_ECB, _dst_buf,
                   _long_buf, sizeof(_PLAINTEXT_ECB), 1u);
}

TEST(dma_aes_poll, ecb_long)
{
    // test a long buffer, which is a repeated version of the short one.
    // also take the opportunity to test src == dst buffers
    size_t repeat = sizeof(_long_buf)/sizeof(_PLAINTEXT_ECB);
    uint8_t * ptr = _long_buf;
    for (unsigned int ix=0; ix<repeat; ix++) {
        memcpy(ptr, _PLAINTEXT_ECB, sizeof(_PLAINTEXT_ECB));
        ptr += sizeof(_PLAINTEXT_ECB);
    }
    _test_dma_poll(_CIPHERTEXT_ECB, _PLAINTEXT_ECB, _long_buf, _long_buf,
                   sizeof(_long_buf), repeat);
}

TEST_GROUP_RUNNER(dma_aes_poll)
{
    RUN_TEST_CASE(dma_aes_poll, ecb_short);
    RUN_TEST_CASE(dma_aes_poll, ecb_long);
}

TEST_GROUP(dma_aes_irq);

TEST_SETUP(dma_aes_irq)
{
    _hca_irq_init(&_work);
}

TEST_TEAR_DOWN(dma_aes_irq)
{
    _hca_irq_fini();
}

TEST(dma_aes_irq, ecb_short)
{
    memcpy(_long_buf, _PLAINTEXT_ECB, sizeof(_PLAINTEXT_ECB));
    _test_dma_irq(_CIPHERTEXT_ECB, _PLAINTEXT_ECB, _dst_buf,
                  _long_buf, sizeof(_PLAINTEXT_ECB), 1u, &_work);
}

TEST(dma_aes_irq, ecb_long)
{
    // test a long buffer, which is a repeated version of the short one.
    // also take the opportunity to test src == dst buffers
    size_t repeat = sizeof(_long_buf)/sizeof(_PLAINTEXT_ECB);
    uint8_t * ptr = _long_buf;
    for (unsigned int ix=0; ix<repeat; ix++) {
        memcpy(ptr, _PLAINTEXT_ECB, sizeof(_PLAINTEXT_ECB));
        ptr += sizeof(_PLAINTEXT_ECB);
    }
    _test_dma_irq(_CIPHERTEXT_ECB, _PLAINTEXT_ECB, _long_buf, _long_buf,
                 sizeof(_long_buf), repeat, &_work);
}

TEST_GROUP_RUNNER(dma_aes_irq)
{
    RUN_TEST_CASE(dma_aes_irq, ecb_short);
    RUN_TEST_CASE(dma_aes_irq, ecb_long);
}



