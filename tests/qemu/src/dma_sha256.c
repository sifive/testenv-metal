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
#include "dma_test.h"


//-----------------------------------------------------------------------------
// Debug
//-----------------------------------------------------------------------------

#undef SHOW_STEP

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

struct sha_desc {
    struct buf_desc sd_prolog; /**< Send w/o DMA: non-aligned start bytes */
    struct buf_desc sd_main;   /**< Send w/ DMA: aligned payload */
    struct buf_desc sd_finish; /**< Send w/ DMA: remainging paylod + padding */
    struct buf_desc sd_epilog; /**< Send w/o DMA: non-aligned end bytes */
};

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

static const uint8_t _MSG1[] __attribute__((aligned(DMA_ALIGNMENT))) =
{
    0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
    0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
    0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
    0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
    0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
    0xAB, 0xAB, 0xAB, 0xAB
};

static const uint8_t _MSG1_HASH01[] = {
    0x08, 0x7d, 0x80, 0xf7, 0xf1, 0x82, 0xdd, 0x44, 0xf1, 0x84, 0xaa, 0x86,
    0xca, 0x34, 0x48, 0x88, 0x53, 0xeb, 0xcc, 0x04, 0xf0, 0xc6, 0x0d, 0x52,
    0x94, 0x91, 0x9a, 0x46, 0x6b, 0x46, 0x38, 0x31,
};

static const uint8_t _MSG1_HASH55[] = {
    0X48, 0XD7, 0X6E, 0XAB, 0X30, 0XE5, 0X12, 0X01, 0XF4, 0XF0, 0X3E, 0XC7,
    0XA8, 0X5D, 0XAB, 0X85, 0X10, 0XFB, 0X34, 0X09, 0XCC, 0XD1, 0X5B, 0X54,
    0X76, 0X7F, 0X9B, 0X44, 0X35, 0XC9, 0XF5, 0X4D,
};

static const uint8_t _MSG1_HASH56[] = {
    0XA8, 0XC9, 0X90, 0X6A, 0XDE, 0X2A, 0X2E, 0XFF, 0X86, 0X8F, 0XD8, 0XF9,
    0X7A, 0X57, 0X0B, 0XBC, 0X01, 0XA1, 0X3C, 0XDD, 0XC3, 0X2C, 0X3D, 0XFD,
    0XC9, 0XA1, 0X8F, 0X06, 0X18, 0XD6, 0X9E, 0X55,
};

static const uint8_t _MSG1_HASH57[] = {
    0X21, 0XD0, 0X63, 0X69, 0X3F, 0XBB, 0XA4, 0X4F, 0X9F, 0XFA, 0X96, 0X64,
    0X66, 0XE2, 0XF9, 0X4D, 0X99, 0X31, 0XB9, 0XC9, 0X51, 0X91, 0X20, 0XC3,
    0X80, 0X4E, 0XF1, 0XCE, 0XAF, 0XD9, 0X89, 0XB5,
};

static const uint8_t _MSG1_HASH63[] = {
    0xD1, 0x03, 0x6B, 0xA3, 0x0D, 0x05, 0x0C, 0x74, 0xB1, 0xA5, 0xAB, 0x30,
    0x1F, 0xA2, 0x9F, 0xF0, 0xC6, 0x07, 0xA2, 0x7C, 0xC5, 0x5A, 0xF3, 0x41,
    0x25, 0x77, 0xF7, 0xE0, 0x6D, 0xBD, 0x19, 0x0B,
};

static const uint8_t _MSG1_HASH64[] = {
    0xec, 0x65, 0xc8, 0x79, 0x8e, 0xcf, 0x95, 0x90, 0x24, 0x13, 0xc4, 0x0f,
    0x7b, 0x9e, 0x6d, 0x4b, 0x00, 0x68, 0x88, 0x5f, 0x5f, 0x32, 0x4a, 0xba,
    0x1f, 0x9b, 0xa1, 0xc8, 0xe1, 0x4a, 0xea, 0x61,
};

static const uint8_t _MSG2[] __attribute__((aligned(DMA_ALIGNMENT))) =
{
    0x61, 0x62, 0x63,
};

static const uint8_t _MSG2_HASH[] = {
    0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA, 0x41, 0x41, 0x40, 0xDE,
    0x5D, 0xAE, 0x22, 0x23, 0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
    0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD,
};


//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

static struct worker _work;
static uint8_t _sha2_buf[256u/CHAR_BIT] ALIGN(sizeof(uint64_t));
static uint8_t _src_buf[sizeof(_MSG1)+DMA_ALIGNMENT] ALIGN(DMA_ALIGNMENT);
static uint8_t _trail_buf[2u*SHA256_BLOCK_SIZE] ALIGN(DMA_ALIGNMENT);

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
        if ( desc->sd_prolog.bd_length > length ) {
             desc->sd_prolog.bd_length = length;
        }
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

    // count of bytes to complete a SHA256_BLOCK_SIZE
    size_t to_end = SHA256_BLOCK_SIZE - msg_size%SHA256_BLOCK_SIZE;
    if ( to_end  < SHA256_LEN_SIZE + 1u ) {
        to_end += SHA256_BLOCK_SIZE;
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
            //PRINTF("64 bit push: %zu", length);
            METAL_REG64(HCA_BASE, METAL_SIFIVE_HCA_FIFO_IN) =
                *(const uint64_t *)src;
            src += sizeof(uint64_t);
            length -= sizeof(uint64_t);
            continue;
        }
        #endif // __riscv_xlen >= 64
        if ( ! (((uintptr_t)src) & (sizeof(uint32_t)-1u)) &&
                (length >= sizeof(uint32_t))) {
            //PRINTF("32 bit push: %zu", length);
            METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_FIFO_IN) =
                *(const uint32_t *)src;
            src += sizeof(uint32_t);
            length -= sizeof(uint32_t);
            continue;
        }
        if ( ! (((uintptr_t)src) & (sizeof(uint16_t)-1u)) &&
                (length >= sizeof(uint16_t))) {
            //PRINTF("16 bit push: %zu", length);
            METAL_REG16(HCA_BASE, METAL_SIFIVE_HCA_FIFO_IN) =
                *(const uint16_t *)src;
            src += sizeof(uint16_t);
            length -= sizeof(uint16_t);
            continue;
        }
        if ( ! (((uintptr_t)src) & (sizeof(uint8_t)-1u)) ) {
            //PRINTF("8 bit push: %zu", length);
            METAL_REG8(HCA_BASE, METAL_SIFIVE_HCA_FIFO_IN) =
                *(const uint8_t *)src;
            src += sizeof(uint8_t);
            length -= sizeof(uint8_t);
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

    // SHA mode
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, SHA2_SHA256,
                  HCA_REGISTER_SHA_CR_MODE_OFFSET,
                  HCA_REGISTER_SHA_CR_MODE_MASK);

    if ( _hca_sha_is_busy() ) {
        TEST_FAIL_MESSAGE("SHA HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    // sanity check
    uint32_t hca_cr = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    TEST_ASSERT_EQUAL_MESSAGE(hca_cr & HCA_CR_IFIFO_EMPTY_BIT,
                              HCA_CR_IFIFO_EMPTY_BIT,
                              "FIFO in is not empty");
    TEST_ASSERT_EQUAL_MESSAGE(hca_cr & HCA_CR_IFIFO_FULL_BIT, 0u,
                              "FIFO in is full");

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
    // as there is not HCA reset for now, the easiest way is to change the
    // mode. Note that this may not reflect the way the actual HW behaves..
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 0,
                  HCA_REGISTER_CR_IFIFOTGT_OFFSET,
                  HCA_REGISTER_CR_IFIFOTGT_MASK);
    _hca_updreg32(METAL_SIFIVE_HCA_CR, 1,
                  HCA_REGISTER_CR_IFIFOTGT_OFFSET,
                  HCA_REGISTER_CR_IFIFOTGT_MASK);
}

static void
_test_sha_dma_poll(const uint8_t * refh, const uint8_t * buf, size_t buflen)
{
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
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, SHA2_SHA256,
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
            10u, dma_loop, "VM may have freeze guest code execution");
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

    _hca_sha_get_hash(_sha2_buf, 256u/CHAR_BIT);

    // DUMP_HEX("Hash:", _sha2_buf, 256u/CHAR_BIT);

    if ( refh ) {
        if ( memcmp(_sha2_buf, refh, 256u/CHAR_BIT) ) {
            DUMP_HEX("Invalid hash:", _sha2_buf, 256u/CHAR_BIT);
            DUMP_HEX("Ref:         ", refh, 256u/CHAR_BIT);
            TEST_FAIL_MESSAGE("Hash mismatch");
        }
    }

    // sanity check
    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    TEST_ASSERT_EQUAL_MESSAGE(reg & HCA_CR_IFIFO_EMPTY_BIT,
                              HCA_CR_IFIFO_EMPTY_BIT,
                              "FIFO in is not empty");
    TEST_ASSERT_EQUAL_MESSAGE(reg & HCA_CR_IFIFO_FULL_BIT, 0u,
                              "FIFO in is full");
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
                  struct worker * work)
{
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
    _hca_updreg32(METAL_SIFIVE_HCA_SHA_CR, SHA2_SHA256,
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

    _hca_sha_get_hash(_sha2_buf, 256u/CHAR_BIT);

    if ( refh ) {
        if ( memcmp(_sha2_buf, refh, 256u/CHAR_BIT) ) {
            DUMP_HEX("Invalid hash:", _sha2_buf, 256u/CHAR_BIT);
            DUMP_HEX("Ref:         ", refh, 256u/CHAR_BIT);
            TEST_FAIL_MESSAGE("Hash mismatch");
        }
    }
}

void _test_sha_dma_poll_msg1_size(const uint8_t * hash, size_t length)
{
    _test_sha_dma_poll(hash, _MSG1, length);
    for (unsigned int ix=1; ix<DMA_ALIGNMENT; ix++) {
        memcpy(&_src_buf[ix], _MSG1, length);
        _test_sha_dma_poll(hash, &_src_buf[ix], length);
    }
}

void _test_sha_dma_irq_msg1_size(const uint8_t * hash, size_t length)
{
    _test_sha_dma_irq(hash, _MSG1, length, &_work);
    for (unsigned int ix=1; ix<DMA_ALIGNMENT; ix++) {
        memcpy(&_src_buf[ix], _MSG1, length);
        _test_sha_dma_irq(hash, &_src_buf[ix], length, &_work);
    }
}

//-----------------------------------------------------------------------------
// Unity tests
//-----------------------------------------------------------------------------

TEST_GROUP(dma_sha256_poll);

TEST_SETUP(dma_sha256_poll) {}

TEST_TEAR_DOWN(dma_sha256_poll) {}

TEST(dma_sha256_poll, unaligned)
{
    // note: error behaviour will DMA/IRQ is not defined in HCA documentation
    // it needs to be addressed somehow
    for (unsigned int ix=0; ix<DMA_ALIGNMENT; ix++) {
        _test_sha_dma_unaligned_poll((const uint8_t *)&_MSG1[ix],
                                     DMA_BLOCK_SIZE);
    }
}

TEST(dma_sha256_poll, short_msg1_64)
{
    _test_sha_dma_poll_msg1_size(_MSG1_HASH64, 64u);
}

TEST(dma_sha256_poll, short_msg1_63)
{
    _test_sha_dma_poll_msg1_size(_MSG1_HASH63, 63u);
}

TEST(dma_sha256_poll, short_msg1_1)
{
    _test_sha_dma_poll_msg1_size(_MSG1_HASH01, 1u);
}

TEST(dma_sha256_poll, short_msg1_55)
{
    _test_sha_dma_poll_msg1_size(_MSG1_HASH55, 55u);
}

TEST(dma_sha256_poll, short_msg1_56)
{
    _test_sha_dma_poll_msg1_size(_MSG1_HASH56, 56u);
}

TEST(dma_sha256_poll, short_msg1_57)
{
    _test_sha_dma_poll_msg1_size(_MSG1_HASH57, 57u);
}

TEST(dma_sha256_poll, short_msg2)
{
    _test_sha_dma_poll(_MSG2_HASH, (const uint8_t *)_MSG2, sizeof(_MSG2));
    for (unsigned int ix=1; ix<DMA_ALIGNMENT; ix++) {
        memcpy(&_src_buf[ix], _MSG2, sizeof(_MSG2));
        _test_sha_dma_poll(_MSG2_HASH, &_src_buf[ix], sizeof(_MSG2));
    }
}

TEST_GROUP_RUNNER(dma_sha256_poll)
{
    RUN_TEST_CASE(dma_sha256_poll, unaligned);
    RUN_TEST_CASE(dma_sha256_poll, short_msg1_64);
    RUN_TEST_CASE(dma_sha256_poll, short_msg1_63);
    RUN_TEST_CASE(dma_sha256_poll, short_msg1_1);
    RUN_TEST_CASE(dma_sha256_poll, short_msg1_55);
    RUN_TEST_CASE(dma_sha256_poll, short_msg1_56);
    RUN_TEST_CASE(dma_sha256_poll, short_msg1_57);
    RUN_TEST_CASE(dma_sha256_poll, short_msg2);
}

TEST_GROUP(dma_sha256_irq);

TEST_SETUP(dma_sha256_irq)
{
    _hca_irq_init(&_work);
}

TEST_TEAR_DOWN(dma_sha256_irq)
{
    _hca_irq_fini();
}

TEST(dma_sha256_irq, short_msg1_64)
{
    _test_sha_dma_irq_msg1_size(_MSG1_HASH64, 64u);
}

TEST(dma_sha256_irq, short_msg1_63)
{
    _test_sha_dma_irq_msg1_size(_MSG1_HASH63, 63u);
}

TEST(dma_sha256_irq, short_msg1_1)
{
    _test_sha_dma_irq_msg1_size(_MSG1_HASH01, 1u);
}

TEST(dma_sha256_irq, short_msg1_55)
{
    _test_sha_dma_irq_msg1_size(_MSG1_HASH55, 55u);
}

TEST(dma_sha256_irq, short_msg1_56)
{
    _test_sha_dma_irq_msg1_size(_MSG1_HASH56, 56u);
}

TEST(dma_sha256_irq, short_msg1_57)
{
    _test_sha_dma_irq_msg1_size(_MSG1_HASH57, 57u);
}

TEST(dma_sha256_irq, short_msg2)
{
    _test_sha_dma_irq(_MSG2_HASH, (const uint8_t *)_MSG2, sizeof(_MSG2),
                      &_work);
    for (unsigned int ix=1; ix<DMA_ALIGNMENT; ix++) {
        memcpy(&_src_buf[ix], _MSG2, sizeof(_MSG2));
        _test_sha_dma_irq(_MSG2_HASH, &_src_buf[ix], sizeof(_MSG2), &_work);
    }
}

TEST_GROUP_RUNNER(dma_sha256_irq)
{
    RUN_TEST_CASE(dma_sha256_irq, short_msg1_64);
    RUN_TEST_CASE(dma_sha256_irq, short_msg1_63);
    RUN_TEST_CASE(dma_sha256_irq, short_msg1_1);
    RUN_TEST_CASE(dma_sha256_irq, short_msg1_55);
    RUN_TEST_CASE(dma_sha256_irq, short_msg1_56);
    RUN_TEST_CASE(dma_sha256_irq, short_msg1_57);
    RUN_TEST_CASE(dma_sha256_irq, short_msg2);
}
