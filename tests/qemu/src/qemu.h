#ifndef QEMU_H
#define QEMU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "api/hardware/v0.5/random/hca_trng.h"
#include "api/hardware/v0.5/sifive_hca-0.5.x.h"
#include "api/hardware/hca_utils.h"
#include "api/hardware/hca_macro.h"

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

/* A tasklet executed by a Hard */
typedef void (* qemu_hart_task_t)(void);

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define TIME_BASE            32768u // cannot rely on metal API for now
#define HEART_BEAT_FREQUENCY 32u
#define HEART_BEAT_TIME      ((TIME_BASE)/(HEART_BEAT_FREQUENCY))

#define PAGE_SIZE            4096  // bytes

#define MAX_HARTS            16u

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a_) (sizeof((_a_))/sizeof((_a_)[0]))
#endif // ARRAY_SIZE

#define ALIGN(_a_) __attribute__((aligned((_a_))))

#ifndef METAL_REG32
# define METAL_REG32(base, offset) \
    (__METAL_ACCESS_ONCE((uint32_t *)((base) + (offset))))
#endif

#ifndef METAL_REG16
# define METAL_REG16(base, offset) \
    (__METAL_ACCESS_ONCE((uint16_t *)((base) + (offset))))
#endif

#ifndef METAL_REG8
# define METAL_REG8(base, offset) \
    (__METAL_ACCESS_ONCE((uint8_t *)((base) + (offset))))
#endif

#ifndef MIN
# define MIN(_a_, _b_) ((_a_) < (_b_) ? (_a_) : (_b_))
#endif // MIN

#ifndef MAX
# define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))
#endif // MAX

#ifndef QUIET
# define LPRINTF(_f_, _l_, _msg_, ...) \
    printf("%s[%d] " _msg_ "\n", _f_, _l_, ##__VA_ARGS__)
# define PRINTF(_msg_, ...) \
    LPRINTF(__func__, __LINE__, _msg_, ##__VA_ARGS__)
#else // QUIET
# define LPRINTF(_f_, _l_, _msg_, ...)
# define PRINTF(_msg_, ...)
#endif

#define HEX_LINE_LEN  32u

#define DUMP_HEX(_msg_, _buf_, _len_) \
   qemu_hexdump(__func__, __LINE__, _msg_, _buf_, _len_);
#define DUMP_SHEX(_msg_, _buf_, _len_) \
   qemu_hexdump(NULL, 0, _msg_, _buf_, _len_);

#define TEST_TIMEOUT(_to_, _msg_) \
    TEST_ASSERT_LESS_THAN_UINT64_MESSAGE((_to_), now(), _msg_);

// this is HCA-specific, should be moved...
#define DMA_ALIGNMENT        32u   // bytes
#define DMA_BLOCK_SIZE       16u   // bytes

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

extern uint8_t ALIGN(DMA_ALIGNMENT) dma_long_buf[4*PAGE_SIZE];

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

void qemu_hexdump(const char * func, int line, const char * msg,
                  const uint8_t *buf, size_t size);

void qemu_register_hart_task(unsigned int hartid, qemu_hart_task_t task);

//-----------------------------------------------------------------------------
// Inline helpers
//-----------------------------------------------------------------------------

static inline uint64_t
now(void)
{
    return (uint64_t)
        metal_cpu_get_mtime(metal_cpu_get(metal_cpu_get_current_hartid()));
}

static inline uint64_t
ms_to_ts(unsigned int ms)
{
    return (TIME_BASE * (uint64_t)ms) / 1000ull;
}

#endif // QEMU_H