#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "metal/machine.h"
#include "metal/tty.h"
#include "unity_fixture.h"
#include "dma_test.h"


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

static const uint8_t _KEY_AES128[] = {
    0x48, 0xB7, 0xF3, 0x37, 0xCD, 0xF9, 0x25, 0x26,
    0x87, 0xEC, 0xC7, 0x60, 0xBD, 0x8E, 0xC1, 0x84
};

static const uint8_t _INITV_AES96[] = {
    0x3E, 0x89, 0x4E, 0xBB, 0x16, 0xCE, 0x82, 0xA5, 0x3C, 0x3E, 0x05, 0xB2
};

static const uint8_t _AAD_GCM[] ALIGN(DMA_ALIGNMENT) = {
    0x7D, 0x92, 0x4C, 0xFD, 0x37, 0xB3, 0xD0, 0x46, 0xA9, 0x6E, 0xB5, 0xE1,
    0x32, 0x04, 0x24, 0x05, 0xC8, 0x73, 0x1E, 0x06, 0x50, 0x97, 0x87, 0xBB,
    0xEB, 0x41, 0xF2, 0x58, 0x27, 0x57, 0x46, 0x49, 0x5E, 0x88, 0x4D, 0x69,
    0x87, 0x1F, 0x77, 0x63, 0x4C, 0x58, 0x4B, 0xB0, 0x07, 0x31, 0x22, 0x34
};

static const uint8_t _PLAINTEXT_GCM[] ALIGN(DMA_ALIGNMENT) = {
    0xBB, 0x2B, 0xAC, 0x67, 0xA4, 0x70, 0x94, 0x30,
    0xC3, 0x9C, 0x2E, 0xB9, 0xAC, 0xFA, 0xBC, 0x0D,
    0x45, 0x6C, 0x80, 0xD3, 0x0A, 0xA1, 0x73, 0x4E,
    0x57, 0x99, 0x7D, 0x54, 0x8A, 0x8F, 0x06, 0x03
};

static const uint8_t _CIPHERTEXT_GCM[] ALIGN(DMA_ALIGNMENT) = {
    0xD2, 0x63, 0x22, 0x8B, 0x8C, 0xE0, 0x51, 0xF6,
    0x7E, 0x9B, 0xAF, 0x1C, 0xE7, 0xDF, 0x97, 0xD1,
    0x0C, 0xD5, 0xF3, 0xBC, 0x97, 0x23, 0x62, 0x05,
    0x51, 0x30, 0xC7, 0xD1, 0x3C, 0x3A, 0xB2, 0xE7
};

static const uint8_t _TAG_GCM[] ALIGN(DMA_ALIGNMENT) = {
    0x71, 0x44, 0x67, 0x37, 0xCA, 0x1F, 0xA9, 0x2E,
    0x6D, 0x02, 0x6D, 0x7D, 0x2E, 0xD1, 0xAA, 0x9C
};

// the following constants are generated with the companion dma_aes_gcm.py
// script

static const uint8_t _KEY_GCM2[16u] = {
    0x90, 0xB0, 0x36, 0x29, 0xA0, 0xDB, 0x1B, 0x41, 0xB5, 0x64, 0x06, 0x24,
    0x0F, 0xC4, 0x38, 0x87
};

static const uint8_t _IV_GCM2[12u] = {
    0xB9, 0xEE, 0xA3, 0xFE, 0xEE, 0x90, 0x1B, 0x03, 0xDD, 0xF7, 0xBB, 0x6B
};

static const uint8_t _AAD_GCM2[57u] ALIGN(DMA_ALIGNMENT) = {
    0xC1, 0x3F, 0x2C, 0x41, 0x14, 0x89, 0xF2, 0x24, 0x6D, 0x40, 0xD5, 0x70,
    0x6D, 0xB5, 0x8F, 0x2A, 0xD0, 0x9F, 0x38, 0x84, 0x8E, 0xE5, 0x1E, 0xDE,
    0x87, 0x91, 0x91, 0x72, 0xDA, 0xBB, 0x19, 0x4E, 0x9A, 0x30, 0xE1, 0xE0,
    0x61, 0x38, 0x04, 0xD9, 0xD2, 0xA1, 0xD5, 0x07, 0xD6, 0xF1, 0x7B, 0xB4,
    0xB1, 0xA4, 0xF4, 0x8F, 0xB9, 0x33, 0x3E, 0x5C, 0x40
};

static const uint8_t _PLAINTEXT_GCM2[112u] ALIGN(DMA_ALIGNMENT) = {
    0x5B, 0xE6, 0xDB, 0x17, 0x65, 0x35, 0x28, 0x7C, 0x3C, 0xFC, 0x21, 0xAF,
    0xD7, 0x71, 0xE1, 0x84, 0x72, 0xF3, 0x82, 0x8E, 0xE7, 0x29, 0x5A, 0x22,
    0xA4, 0xBE, 0xA2, 0xBE, 0xA0, 0xE5, 0x68, 0x92, 0x09, 0x43, 0xF2, 0xE6,
    0xE5, 0xA3, 0x5D, 0xB8, 0xC7, 0xB2, 0x02, 0xA1, 0x82, 0xFF, 0xD2, 0x93,
    0x28, 0x94, 0xD6, 0x08, 0xD9, 0x5C, 0xA1, 0x45, 0x9C, 0xE4, 0x20, 0xB8,
    0x5D, 0x75, 0xEA, 0x28, 0x50, 0x62, 0x71, 0x2A, 0xA0, 0xA0, 0xEB, 0x6F,
    0x3C, 0xC6, 0x3D, 0xF5, 0xB9, 0xD1, 0x37, 0x48, 0x69, 0xC7, 0x39, 0x8A,
    0x28, 0xC1, 0x70, 0xE5, 0x7C, 0x45, 0x44, 0x87, 0x94, 0xCA, 0xE6, 0x9A,
    0xD4, 0xD0, 0x29, 0x77, 0x7E, 0xEE, 0x1A, 0x00, 0xB7, 0x5A, 0x12, 0x0B,
    0x1D, 0x6D, 0x42, 0xF5
};

static const uint8_t _CIPHERTEXT_GCM2[112u] ALIGN(DMA_ALIGNMENT) = {
    0x7F, 0xB2, 0x7B, 0x4D, 0x78, 0xF1, 0x1D, 0x48, 0x85, 0x6F, 0x0B, 0x4C,
    0x9F, 0xF0, 0x20, 0x88, 0x54, 0xDF, 0xEE, 0x6C, 0xEF, 0x21, 0x8B, 0x02,
    0x28, 0x4C, 0x84, 0x78, 0x8B, 0x6D, 0x84, 0x9F, 0x83, 0x01, 0xF8, 0x63,
    0x1F, 0xCF, 0x56, 0xC6, 0x23, 0x49, 0x62, 0x83, 0x51, 0x92, 0x4E, 0xA6,
    0xDA, 0x19, 0xDF, 0x2F, 0x88, 0xCE, 0x2B, 0xBF, 0x5E, 0x9B, 0x9D, 0xF1,
    0xBA, 0xB4, 0x71, 0x88, 0x6D, 0x22, 0xBA, 0xCB, 0xF0, 0x1F, 0xD3, 0x38,
    0xBA, 0xDE, 0xAF, 0x05, 0xE5, 0x21, 0xB4, 0x58, 0x25, 0xCE, 0x39, 0x1A,
    0xEB, 0x7C, 0x19, 0x56, 0x70, 0x4B, 0x03, 0x2E, 0x95, 0x22, 0x08, 0x49,
    0x07, 0xC1, 0x33, 0xB4, 0x8E, 0xA6, 0xDA, 0x0D, 0x55, 0x63, 0x9C, 0xD2,
    0x24, 0x13, 0x01, 0x93
};

static const uint8_t _TAG_GCM2[16u] = {
    0x2E, 0xD6, 0xDE, 0x55, 0xD8, 0x39, 0x24, 0xEE, 0x22, 0xD2, 0x69, 0xF0,
    0xFB, 0x4D, 0xEE, 0x54
};

// alias
#define METAL_SIFIVE_HCA_FIFO_OUT (METAL_SIFIVE_HCA_AES_OUT)

//-----------------------------------------------------------------------------
// Debug
//-----------------------------------------------------------------------------

#undef SHOW_STEP

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

struct aes_desc {
    struct buf_desc ad_prolog; /**< Send w/o DMA: non-aligned start bytes */
    struct buf_desc ad_main;   /**< Send w/ DMA: aligned payload */
    struct buf_desc ad_epilog; /**< Send w/o DMA: non-aligned end bytes */
};

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

static struct worker _work;
static uint8_t _dst_buf[sizeof(_CIPHERTEXT_GCM2)+DMA_BLOCK_SIZE]
    ALIGN(DMA_ALIGNMENT);
static uint8_t _aad_buf[sizeof(_AAD_GCM2)+2*DMA_ALIGNMENT] \
    ALIGN(DMA_ALIGNMENT);
static uint8_t _tag_buf[AES_BLOCK_SIZE] ALIGN(sizeof(uint32_t));

//-----------------------------------------------------------------------------
// DMA AES test implementation
//-----------------------------------------------------------------------------

static void
_test_dma_aligned(uint8_t * dst, uint8_t * tag,
                  const uint8_t * src, size_t src_len,
                  const uint8_t * aad, size_t aad_len)
{
    uint32_t reg;

    TEST_ASSERT_EQUAL_MESSAGE(((uintptr_t)src) & ((DMA_ALIGNMENT) - 1u), 0,
                              "Source is not aligned on a DMA boundary");
    TEST_ASSERT_EQUAL_MESSAGE(((uintptr_t)dst) & ((DMA_ALIGNMENT) - 1u), 0,
                              "Destination is not aligned on a DMA boundary");
    TEST_ASSERT_EQUAL_MESSAGE(((uintptr_t)aad) & ((DMA_ALIGNMENT) - 1u), 0,
                              "Aad is not aligned on a DMA boundary");
    TEST_ASSERT_EQUAL_MESSAGE(((uintptr_t)tag) & ((sizeof(uint32_t)) - 1u), 0,
                              "Tag is not aligned on a word");
    TEST_ASSERT_EQUAL_MESSAGE(src_len & (DMA_BLOCK_SIZE - 1u), 0,
                              "Length is not aligned on a DMA block size");
    TEST_ASSERT_EQUAL_MESSAGE(aad_len & (DMA_BLOCK_SIZE - 1u), 0,
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

    // AES mode: GCM
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 5u,
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

    // AES IV
    _hca_set_aes_iv96(_INITV_AES96);

    if ( _hca_aes_is_busy() ) {
        TEST_FAIL_MESSAGE("AES HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    // AES set AAD
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                    HCA_REGISTER_AES_CR_DTYPE_OFFSET,
                    HCA_REGISTER_AES_CR_DTYPE_MASK);

    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_ALEN+0u) = (uint32_t)aad_len;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_ALEN+4u) = 0u;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_PDLEN+0u) = (uint32_t)src_len;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_PDLEN+4u) = 0u;

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) = (uintptr_t)aad;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = (uintptr_t)0;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = aad_len/DMA_BLOCK_SIZE;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1u,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    size_t dma_loop = 0;
    while( _hca_dma_is_busy() ) {
        // busy loop
        dma_loop += 1;
    }

    if ( aad_len > 4096u ) {
        // whenever the buffer is greater than the VM chunk size, we expect
        // the guest code to be re-scheduled before the VM DMA completion
        TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(
            1000u, dma_loop, "VM may have freeze guest code execution");
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

    // AES set Payload
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 1u,
                    HCA_REGISTER_AES_CR_DTYPE_OFFSET,
                    HCA_REGISTER_AES_CR_DTYPE_MASK);

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) = (uintptr_t)src;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = (uintptr_t)dst;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) = src_len/DMA_BLOCK_SIZE;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1u,
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

    if ( src_len > 4096u ) {
        // whenever the buffer is greater than the VM chunk size, we expect
        // the guest code to be re-scheduled before the VM DMA completion
        TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(
            1000u, dma_loop, "VM may have freeze guest code execution");
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

    for (unsigned int ix=0; ix<AES_BLOCK_SIZE; ix+=sizeof(uint32_t)) {
        *(uint32_t *)&tag[AES_BLOCK_SIZE - sizeof(uint32_t) - ix] =
            __builtin_bswap32(
                METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_AUTH+ix));
    }
}

static int
_build_aes_desc(struct aes_desc * desc, const uint8_t * src, size_t length)
{
    size_t unaligned_size = (uintptr_t)src & (DMA_ALIGNMENT - 1u);
    size_t data_size = length;
    if ( unaligned_size ) {
        desc->ad_prolog.bd_base = src;
        desc->ad_prolog.bd_length = DMA_ALIGNMENT - unaligned_size;
        src += desc->ad_prolog.bd_length;
        length -= desc->ad_prolog.bd_length;
    } else {
        desc->ad_prolog.bd_base = NULL;
        desc->ad_prolog.bd_length = 0u;
    }

    desc->ad_main.bd_base = src;
    desc->ad_main.bd_count = length / DMA_BLOCK_SIZE;
    size_t main_length = desc->ad_main.bd_count * DMA_BLOCK_SIZE;
    src += main_length;
    length -= main_length;

    if ( length ) {
        desc->ad_epilog.bd_base = src;
        desc->ad_epilog.bd_length = length;
    } else {
        desc->ad_epilog.bd_base = NULL;
        desc->ad_epilog.bd_count = 0;
    }


    return 0;
}

#ifdef SHOW_STEP
static void
_show_desc(const char * name, const uint8_t * src, size_t length,
           struct aes_desc * desc)
{
    PRINTF("");
    PRINTF("Desc: %s: %p %zu", name, src, length);
    PRINTF("Prolog: %p %zu", desc->ad_prolog.bd_base,
           desc->ad_prolog.bd_length);
    PRINTF("Main:   %p %zu [%zu]", desc->ad_main.bd_base,
        desc->ad_main.bd_count*DMA_BLOCK_SIZE, desc->ad_main.bd_count);
    PRINTF("Epilog: %p %zu", desc->ad_epilog.bd_base,
           desc->ad_epilog.bd_length);
}
#else
# define _show_desc(_n_, _s_, _l_, _d_)
#endif

static void
_fifo_in_push(const uint8_t * src, size_t length)
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
_fifo_out_pop(uint8_t * dst, size_t length)
{
    const uint8_t * end = dst+length;
    while ( dst < end ) {
        #if __riscv_xlen >= 64
        if ( !(((uintptr_t)dst) & (sizeof(uint64_t)-1u)) &&
                (length >= sizeof(uint64_t))) {
            //PRINTF("64 bit pop: %zu", length);
            *(uint64_t *)dst =
                METAL_REG64(HCA_BASE, METAL_SIFIVE_HCA_FIFO_OUT);
            dst += sizeof(uint64_t);
            length -= sizeof(uint64_t);
            continue;
        }
        #endif // __riscv_xlen >= 64
        if ( ! (((uintptr_t)dst) & (sizeof(uint32_t)-1u)) &&
                (length >= sizeof(uint32_t))) {
            //PRINTF("32 bit pop: %zu", length);
            *(uint32_t *)dst =
                METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_FIFO_OUT);
            dst += sizeof(uint32_t);
            length -= sizeof(uint32_t);
            continue;
        }
        if ( ! (((uintptr_t)dst) & (sizeof(uint16_t)-1u)) &&
                (length >= sizeof(uint16_t))) {
            //PRINTF("16 bit pop: %zu", length);
            *(uint16_t *)dst =
                METAL_REG16(HCA_BASE, METAL_SIFIVE_HCA_FIFO_OUT);
            dst += sizeof(uint16_t);
            length -= sizeof(uint16_t);
            continue;
        }
        if ( ! (((uintptr_t)dst) & (sizeof(uint8_t)-1u)) ) {
            //PRINTF("8 bit pop: %zu", length);
            *(uint8_t *)dst =
                 METAL_REG8(HCA_BASE, METAL_SIFIVE_HCA_FIFO_OUT);
            dst += sizeof(uint8_t);
            length -= sizeof(uint8_t);
            continue;
        }
    }
}

static void
_test_dma_unaligned(uint8_t * dst, uint8_t * tag,
                    const uint8_t * src, size_t src_len,
                    const uint8_t * aad, size_t aad_len)
{
    uintptr_t src_off = ((uintptr_t)src) & ((DMA_BLOCK_SIZE) - 1u);
    uintptr_t dst_off = ((uintptr_t)dst) & ((DMA_ALIGNMENT) - 1u);

    TEST_ASSERT_EQUAL_MESSAGE(src_len & (DMA_BLOCK_SIZE - 1u), 0,
                              "Length is not aligned on a DMA block size");
    TEST_ASSERT_EQUAL_MESSAGE(((uintptr_t)tag) & ((sizeof(uint32_t)) - 1u), 0,
                              "Tag is not aligned on a word");
    // source is not aligned
    TEST_ASSERT_EQUAL_MESSAGE(dst_off, 0,
            "Destination is not aligned on a DMA block size");

    uint32_t reg;

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

    // AES mode: GCM
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 5u,
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
    _hca_set_aes_key128(_KEY_GCM2);

    // AES IV
    _hca_set_aes_iv96(_IV_GCM2);

    if ( _hca_aes_is_busy() ) {
        TEST_FAIL_MESSAGE("AES HW is busy");
    }

    if ( _hca_dma_is_busy() ) {
        TEST_FAIL_MESSAGE("DMA HW is busy");
    }

    struct aes_desc aad_desc;
    struct aes_desc pld_desc;
    struct aes_desc ciph_desc;

    _build_aes_desc(&aad_desc, aad, aad_len);
    _build_aes_desc(&pld_desc, src, src_len);
    _build_aes_desc(&ciph_desc, dst, src_len);

    _show_desc("AAD", aad, aad_len, &aad_desc);
    _show_desc("IN ", aad, src_len, &pld_desc);
    _show_desc("OUT", dst, src_len, &ciph_desc);

    if ( ciph_desc.ad_main.bd_count > pld_desc.ad_main.bd_count ) {
        size_t count = ciph_desc.ad_main.bd_count - pld_desc.ad_main.bd_count;
        ciph_desc.ad_main.bd_count -= count;
        size_t length = count*DMA_BLOCK_SIZE;
        if ( ! ciph_desc.ad_epilog.bd_base ) {
            ciph_desc.ad_epilog.bd_base = ciph_desc.ad_main.bd_base +
                ciph_desc.ad_main.bd_count * DMA_BLOCK_SIZE;
        } else {
            ciph_desc.ad_epilog.bd_base -= length;
        }
        ciph_desc.ad_epilog.bd_length += length;
        _show_desc("OUT", dst, src_len, &ciph_desc);
    }

    // AES set AAD
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 0u,
                    HCA_REGISTER_AES_CR_DTYPE_OFFSET,
                    HCA_REGISTER_AES_CR_DTYPE_MASK);

    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_ALEN+0u) = (uint32_t)aad_len;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_ALEN+4u) = 0u;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_PDLEN+0u) = (uint32_t)src_len;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_PDLEN+4u) = 0u;

    if ( aad_desc.ad_prolog.bd_length ) {
        _fifo_in_push(aad_desc.ad_prolog.bd_base,
                      aad_desc.ad_prolog.bd_length);
    }

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =
        (uintptr_t)aad_desc.ad_main.bd_base;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) = (uintptr_t)0;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) =
        aad_desc.ad_main.bd_count;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1u,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    size_t dma_loop = 0;
    while( _hca_dma_is_busy() ) {
        // busy loop
        dma_loop += 1;
    }


    if ( aad_desc.ad_epilog.bd_length ) {
        _fifo_in_push(aad_desc.ad_epilog.bd_base,
                      aad_desc.ad_epilog.bd_length);
    }

    if ( aad_len > 4096u ) {
        // whenever the buffer is greater than the VM chunk size, we expect
        // the guest code to be re-scheduled before the VM DMA completion
        TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(
            1000u, dma_loop, "VM may have freeze guest code execution");
    }

    // sanity check
    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    TEST_ASSERT_EQUAL_MESSAGE(reg &
                              (HCA_CR_IFIFO_FULL_BIT|HCA_CR_OFIFO_FULL_BIT),
                              0u,
                              "FIFOs are full");

    // AES set Payload
    _hca_updreg32(METAL_SIFIVE_HCA_AES_CR, 1u,
                    HCA_REGISTER_AES_CR_DTYPE_OFFSET,
                    HCA_REGISTER_AES_CR_DTYPE_MASK);

    if ( pld_desc.ad_prolog.bd_length ) {
        _fifo_in_push(pld_desc.ad_prolog.bd_base,
                      pld_desc.ad_prolog.bd_length);
    }
    reg = METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_CR);
    if ( pld_desc.ad_prolog.bd_length < AES_BLOCK_SIZE ) {
        // the HW output FIFO should contain no data, as less than an AES
        // block size has been pushed in
        TEST_ASSERT_EQUAL_MESSAGE(reg & HCA_CR_OFIFO_EMPTY_BIT,
                                  HCA_CR_OFIFO_EMPTY_BIT,
                                  "FIFO out is not empty");
    } else {
        TEST_ASSERT_EQUAL_MESSAGE(reg & HCA_CR_OFIFO_EMPTY_BIT, 0,
                                  "FIFO out is empty");
    }

    // DMA config
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_SRC) =
        (uintptr_t)pld_desc.ad_main.bd_base;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_DEST) =
        (uintptr_t)ciph_desc.ad_main.bd_base;
    METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_DMA_LEN) =
        pld_desc.ad_main.bd_count;

    // DMA start
    _hca_updreg32(METAL_SIFIVE_HCA_DMA_CR, 1u,
                  HCA_REGISTER_DMA_CR_START_OFFSET,
                  HCA_REGISTER_DMA_CR_START_MASK);

    dma_loop = 0;
    while( _hca_dma_is_busy() ) {
        // busy loop
        dma_loop += 1;
    }

    if ( pld_desc.ad_epilog.bd_length ) {
        _fifo_in_push(pld_desc.ad_epilog.bd_base,
                      pld_desc.ad_epilog.bd_length);
    }

    while ( _hca_aes_is_busy() ) {
        // busy loop
    }

    if ( src_len > 4096u ) {
        // whenever the buffer is greater than the VM chunk size, we expect
        // the guest code to be re-scheduled before the VM DMA completion
        TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(
            1000u, dma_loop, "VM may have freeze guest code execution");
    }

    if ( ciph_desc.ad_epilog.bd_length ) {
        _fifo_out_pop(ciph_desc.ad_epilog.bd_dest,
                      ciph_desc.ad_epilog.bd_length);
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

    for (unsigned int ix=0; ix<AES_BLOCK_SIZE; ix+=sizeof(uint32_t)) {
        *(uint32_t *)&tag[AES_BLOCK_SIZE - sizeof(uint32_t) - ix] =
            __builtin_bswap32(
                METAL_REG32(HCA_BASE, METAL_SIFIVE_HCA_AES_AUTH+ix));
    }
}

#if 0

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

    // AES mode: GCM
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

#endif

//-----------------------------------------------------------------------------
// Unity tests
//-----------------------------------------------------------------------------

TEST_GROUP(dma_aes_gcm_poll);

TEST_SETUP(dma_aes_gcm_poll) {}

TEST_TEAR_DOWN(dma_aes_gcm_poll) {}

TEST(dma_aes_gcm_poll, aligned)
{
    memset(_dst_buf, 0, sizeof(_CIPHERTEXT_GCM));
    memset(_tag_buf, 0, sizeof(_TAG_GCM));
    _test_dma_aligned(_dst_buf, _tag_buf, _PLAINTEXT_GCM,
                      sizeof(_PLAINTEXT_GCM), _AAD_GCM, sizeof(_AAD_GCM));
    if ( memcmp(_dst_buf, _CIPHERTEXT_GCM, sizeof(_CIPHERTEXT_GCM))) {
        DUMP_HEX("Invalid AES:", _dst_buf, sizeof(_CIPHERTEXT_GCM));
        DUMP_HEX("Ref AES:    ", _CIPHERTEXT_GCM, sizeof(_CIPHERTEXT_GCM));
        TEST_FAIL_MESSAGE("AES encryption mismatch");
    }
    if ( memcmp(_tag_buf, _TAG_GCM, sizeof(_TAG_GCM))) {
        DUMP_HEX("Invalid TAG:", _tag_buf, sizeof(_TAG_GCM));
        DUMP_HEX("Ref tag:    ", _TAG_GCM, sizeof(_TAG_GCM));
        TEST_FAIL_MESSAGE("AES tag mismatch");
    }
}

TEST(dma_aes_gcm_poll, unaligned_aad)
{
    for (unsigned int ix=0; ix<2*DMA_ALIGNMENT; ix++) {
        memset(_dst_buf, 0, sizeof(_dst_buf));
        memset(_tag_buf, 0, sizeof(_tag_buf));
        memset(_aad_buf, 0, sizeof(_aad_buf));
        uint8_t * aad = &_aad_buf[ix];
        memcpy(aad, _AAD_GCM2, sizeof(_AAD_GCM2));
        _test_dma_unaligned(_dst_buf, _tag_buf, _PLAINTEXT_GCM2,
                            sizeof(_PLAINTEXT_GCM2), aad, sizeof(_AAD_GCM2));
        if ( memcmp(_dst_buf, _CIPHERTEXT_GCM2, sizeof(_CIPHERTEXT_GCM2))) {
            DUMP_HEX("Invalid AES:", _dst_buf, sizeof(_CIPHERTEXT_GCM2));
            DUMP_HEX("Ref AES:    ", _CIPHERTEXT_GCM2,
                    sizeof(_CIPHERTEXT_GCM2));
            TEST_FAIL_MESSAGE("AES encryption mismatch");
        }
        if ( memcmp(_tag_buf, _TAG_GCM2, sizeof(_TAG_GCM2))) {
            DUMP_HEX("Invalid TAG:", _tag_buf, sizeof(_TAG_GCM2));
            DUMP_HEX("Ref tag:    ", _TAG_GCM2, sizeof(_TAG_GCM2));
            TEST_FAIL_MESSAGE("AES tag mismatch");
        }
    }
}

TEST(dma_aes_gcm_poll, unaligned_src)
{
    for (unsigned int ix=0; ix<2*DMA_ALIGNMENT; ix++) {
        memset(_dst_buf, 0, sizeof(_CIPHERTEXT_GCM2));
        memset(_tag_buf, 0, sizeof(_tag_buf));
        //memset(_aad_buf, 0, sizeof(_aad_buf));
        uint8_t * src = &dma_long_buf[ix];
        memcpy(src, _PLAINTEXT_GCM2, sizeof(_PLAINTEXT_GCM2));
        memcpy(_aad_buf, _AAD_GCM2, sizeof(_AAD_GCM2));
        #if 0
        PRINTF("");
        PRINTF("src %p..%p dst %p..%p",
               src, src+sizeof(_PLAINTEXT_GCM2),
               _dst_buf, _dst_buf+sizeof(_CIPHERTEXT_GCM2));
        PRINTF("aad %p..%p tag %p..%p",
               _aad_buf, _aad_buf+sizeof(_AAD_GCM2),
               _tag_buf, _tag_buf+16u);
        #endif
        _test_dma_unaligned(_dst_buf, _tag_buf,
                            src, sizeof(_PLAINTEXT_GCM2),
                            _aad_buf, sizeof(_AAD_GCM2));
        if ( memcmp(_dst_buf, _CIPHERTEXT_GCM2, sizeof(_CIPHERTEXT_GCM2))) {
            DUMP_SHEX("Invalid AES:", _dst_buf, sizeof(_CIPHERTEXT_GCM2));
            DUMP_SHEX("Ref AES:    ", _CIPHERTEXT_GCM2,
                      sizeof(_CIPHERTEXT_GCM2));
            TEST_FAIL_MESSAGE("AES encryption mismatch");
        }
        if ( memcmp(_tag_buf, _TAG_GCM2, sizeof(_TAG_GCM2))) {
            DUMP_SHEX("Invalid TAG:", _tag_buf, sizeof(_TAG_GCM2));
            DUMP_SHEX("Ref tag:    ", _TAG_GCM2, sizeof(_TAG_GCM2));
            TEST_FAIL_MESSAGE("AES tag mismatch");
        }
    }
}

TEST_GROUP_RUNNER(dma_aes_gcm_poll)
{
    //RUN_TEST_CASE(dma_aes_gcm_poll, aligned);
    //RUN_TEST_CASE(dma_aes_gcm_poll, unaligned_aad);
    RUN_TEST_CASE(dma_aes_gcm_poll, unaligned_src);
}

TEST_GROUP(dma_aes_gcm_irq);

TEST_SETUP(dma_aes_gcm_irq)
{
    //_hca_irq_init(&_work);
}

TEST_TEAR_DOWN(dma_aes_gcm_irq)
{
    //_hca_irq_fini();
}

#if 0
TEST(dma_aes_gcm_irq, short)
{
    memcpy(dma_long_buf, _PLAINTEXT_GCM, sizeof(_PLAINTEXT_GCM));
    _test_dma_irq(_CIPHERTEXT_GCM, _PLAINTEXT_GCM, _dst_buf,
                  dma_long_buf, sizeof(_PLAINTEXT_GCM), 1u, &_work);
}

TEST(dma_aes_gcm_irq, long)
{
    // test a long buffer, which is a repeated version of the short one.
    // also take the opportunity to test src == dst buffers
    size_t repeat = sizeof(dma_long_buf)/sizeof(_PLAINTEXT_GCM);
    uint8_t * ptr = dma_long_buf;
    for (unsigned int ix=0; ix<repeat; ix++) {
        memcpy(ptr, _PLAINTEXT_GCM, sizeof(_PLAINTEXT_GCM));
        ptr += sizeof(_PLAINTEXT_GCM);
    }
    _test_dma_irq(_CIPHERTEXT_GCM, _PLAINTEXT_GCM, dma_long_buf, dma_long_buf,
                 sizeof(dma_long_buf), repeat, &_work);
}
#endif

TEST_GROUP_RUNNER(dma_aes_gcm_irq)
{
    //RUN_TEST_CASE(dma_aes_gcm_irq, short);
    //RUN_TEST_CASE(dma_aes_gcm_irq, long);
}

