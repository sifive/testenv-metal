/* Copyright 2018 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <metal/platform.h>

#ifdef METAL_SIFIVE_SPI0

#include <metal/clock.h>
#include <metal/cpu.h>
#include <metal/io.h>
#include <metal/private/metal_private_sifive_spi0.h>
#include <metal/time.h>

/* Register fields */
#define METAL_SPI_SCKDIV_MASK 0xFFF

#define METAL_SPI_SCKMODE_PHA_SHIFT 0
#define METAL_SPI_SCKMODE_POL_SHIFT 1

#define METAL_SPI_CSMODE_MASK 3
#define METAL_SPI_CSMODE_AUTO 0
#define METAL_SPI_CSMODE_HOLD 2
#define METAL_SPI_CSMODE_OFF 3

#define METAL_SPI_PROTO_MASK 3
#define METAL_SPI_PROTO_SINGLE 0
#define METAL_SPI_PROTO_DUAL 1
#define METAL_SPI_PROTO_QUAD 2

#define METAL_SPI_ENDIAN_LSB 4

#define METAL_SPI_DISABLE_RX 8

#define METAL_SPI_FRAME_LEN_SHIFT 16
#define METAL_SPI_FRAME_LEN_MASK (0xF << METAL_SPI_FRAME_LEN_SHIFT)

#define METAL_SPI_TXDATA_FULL (1 << 31)
#define METAL_SPI_RXDATA_EMPTY (1 << 31)
#define METAL_SPI_TXMARK_MASK 7
#define METAL_SPI_TXWM 1
#define METAL_SPI_TXRXDATA_MASK (0xFF)

#define METAL_SPI_INTERVAL_SHIFT 16

#define METAL_SPI_CONTROL_IO 0
#define METAL_SPI_CONTROL_MAPPED 1

#define METAL_SPI_REG(offset) (((unsigned long)control_base + offset))
#define METAL_SPI_REGB(offset)                                                 \
    (__METAL_ACCESS_ONCE((__metal_io_u8 *)METAL_SPI_REG(offset)))
#define METAL_SPI_REGW(offset)                                                 \
    (__METAL_ACCESS_ONCE((__metal_io_u32 *)METAL_SPI_REG(offset)))

#define METAL_SPI_RXDATA_TIMEOUT 1

static struct { uint64_t baud_rate; } spi_state[__METAL_DT_NUM_SPIS];

#define get_index(spi) ((spi).__spi_index)

static int configure_spi(struct metal_spi spi,
                         struct metal_spi_config *config) {
    uintptr_t control_base = dt_spi_data[get_index(spi)].base_addr;

    /* Set protocol */
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) &= ~(METAL_SPI_PROTO_MASK);
    switch (config->protocol) {
    case METAL_SPI_SINGLE:
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) |= METAL_SPI_PROTO_SINGLE;
        break;
    case METAL_SPI_DUAL:
        if (config->multi_wire == MULTI_WIRE_ALL)
            METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) |= METAL_SPI_PROTO_DUAL;
        else
            METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) |= METAL_SPI_PROTO_SINGLE;
        break;
    case METAL_SPI_QUAD:
        if (config->multi_wire == MULTI_WIRE_ALL)
            METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) |= METAL_SPI_PROTO_QUAD;
        else
            METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) |= METAL_SPI_PROTO_SINGLE;
        break;
    default:
        /* Unsupported value */
        return -1;
    }

    /* Set Polarity */
    if (config->polarity) {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_SCKMODE) |=
            (1 << METAL_SPI_SCKMODE_POL_SHIFT);
    } else {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_SCKMODE) &=
            ~(1 << METAL_SPI_SCKMODE_POL_SHIFT);
    }

    /* Set Phase */
    if (config->phase) {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_SCKMODE) |=
            (1 << METAL_SPI_SCKMODE_PHA_SHIFT);
    } else {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_SCKMODE) &=
            ~(1 << METAL_SPI_SCKMODE_PHA_SHIFT);
    }

    /* Set Endianness */
    if (config->little_endian) {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) |= METAL_SPI_ENDIAN_LSB;
    } else {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) &= ~(METAL_SPI_ENDIAN_LSB);
    }

    /* Always populate receive FIFO */
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) &= ~(METAL_SPI_DISABLE_RX);

    /* Set CS Active */
    if (config->cs_active_high) {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSDEF) = 0;
    } else {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSDEF) = 1;
    }

    /* Set frame length */
    if ((METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) & METAL_SPI_FRAME_LEN_MASK) !=
        (8 << METAL_SPI_FRAME_LEN_SHIFT)) {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) &= ~(METAL_SPI_FRAME_LEN_MASK);
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) |=
            (8 << METAL_SPI_FRAME_LEN_SHIFT);
    }

    /* Set CS line */
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSID) = config->csid;

    /* Toggle off memory-mapped SPI flash mode, toggle on programmable IO mode
     * It seems that with this line uncommented, the debugger cannot have access
     * to the chip at all because it assumes the chip is in memory-mapped mode.
     * I have to compile the code with this line commented and launch gdb,
     * reset cores, reset $pc, set *((int *) 0x20004060) = 0, (set the flash
     * interface control register to programmable I/O mode) and then continue
     * Alternative, comment out the "flash" line in openocd.cfg */
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_FCTRL) = METAL_SPI_CONTROL_IO;

    return 0;
}

static void spi_mode_switch(struct metal_spi spi,
                            struct metal_spi_config *config,
                            unsigned int trans_stage) {
    uintptr_t control_base = dt_spi_data[get_index(spi)].base_addr;

    if (config->multi_wire == trans_stage) {
        METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) &= ~(METAL_SPI_PROTO_MASK);
        switch (config->protocol) {
        case METAL_SPI_DUAL:
            METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) |= METAL_SPI_PROTO_DUAL;
            break;
        case METAL_SPI_QUAD:
            METAL_SPI_REGW(METAL_SIFIVE_SPI0_FMT) |= METAL_SPI_PROTO_QUAD;
            break;
        default:
            /* Unsupported value */
            return;
        }
    }
}

int sifive_spi0_transfer(struct metal_spi spi, struct metal_spi_config *config,
                         size_t len, char *tx_buf, char *rx_buf) {
    uintptr_t control_base = dt_spi_data[get_index(spi)].base_addr;
    int rc = 0;
    size_t i = 0;

    rc = configure_spi(spi, config);
    if (rc != 0) {
        return rc;
    }

    /* Hold the chip select line for all len transferred */
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSMODE) &= ~(METAL_SPI_CSMODE_MASK);
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSMODE) |= METAL_SPI_CSMODE_HOLD;

    uint32_t rxdata;

    /* Declare time_t variables to break out of infinite while loop */
    time_t endwait;

    for (i = 0; i < config->cmd_num; i++) {

        while (METAL_SPI_REGW(METAL_SIFIVE_SPI0_TXDATA) & METAL_SPI_TXDATA_FULL)
            ;

        if (tx_buf) {
            METAL_SPI_REGB(METAL_SIFIVE_SPI0_TXDATA) = tx_buf[i];
        } else {
            METAL_SPI_REGB(METAL_SIFIVE_SPI0_TXDATA) = 0;
        }

        endwait = metal_time() + METAL_SPI_RXDATA_TIMEOUT;

        while ((rxdata = METAL_SPI_REGW(METAL_SIFIVE_SPI0_RXDATA)) &
               METAL_SPI_RXDATA_EMPTY) {
            if (metal_time() > endwait) {
                METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSMODE) &=
                    ~(METAL_SPI_CSMODE_MASK);

                return 1;
            }
        }

        if (rx_buf) {
            rx_buf[i] = (char)(rxdata & METAL_SPI_TXRXDATA_MASK);
        }
    }

    /* switch to Dual/Quad mode */
    spi_mode_switch(spi, config, MULTI_WIRE_ADDR_DATA);

    /* Send Addr data */
    for (; i < (config->cmd_num + config->addr_num); i++) {

        while (METAL_SPI_REGW(METAL_SIFIVE_SPI0_TXDATA) & METAL_SPI_TXDATA_FULL)
            ;

        if (tx_buf) {
            METAL_SPI_REGB(METAL_SIFIVE_SPI0_TXDATA) = tx_buf[i];
        } else {
            METAL_SPI_REGB(METAL_SIFIVE_SPI0_TXDATA) = 0;
        }

        endwait = metal_time() + METAL_SPI_RXDATA_TIMEOUT;

        while ((rxdata = METAL_SPI_REGW(METAL_SIFIVE_SPI0_RXDATA)) &
               METAL_SPI_RXDATA_EMPTY) {
            if (metal_time() > endwait) {
                METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSMODE) &=
                    ~(METAL_SPI_CSMODE_MASK);

                return 1;
            }
        }

        if (rx_buf) {
            rx_buf[i] = (char)(rxdata & METAL_SPI_TXRXDATA_MASK);
        }
    }

    /* Send Dummy data */
    for (; i < (config->cmd_num + config->addr_num + config->dummy_num); i++) {

        while (METAL_SPI_REGW(METAL_SIFIVE_SPI0_TXDATA) & METAL_SPI_TXDATA_FULL)
            ;

        if (tx_buf) {
            METAL_SPI_REGB(METAL_SIFIVE_SPI0_TXDATA) = tx_buf[i];
        } else {
            METAL_SPI_REGB(METAL_SIFIVE_SPI0_TXDATA) = 0;
        }

        endwait = metal_time() + METAL_SPI_RXDATA_TIMEOUT;

        while ((rxdata = METAL_SPI_REGW(METAL_SIFIVE_SPI0_RXDATA)) &
               METAL_SPI_RXDATA_EMPTY) {
            if (metal_time() > endwait) {
                METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSMODE) &=
                    ~(METAL_SPI_CSMODE_MASK);
                return 1;
            }
        }
        if (rx_buf) {
            rx_buf[i] = (char)(rxdata & METAL_SPI_TXRXDATA_MASK);
        }
    }

    /* switch to Dual/Quad mode */
    spi_mode_switch(spi, config, MULTI_WIRE_DATA_ONLY);

    for (; i < len; i++) {
        /* Master send bytes to the slave */

        /* Wait for TXFIFO to not be full */
        while (METAL_SPI_REGW(METAL_SIFIVE_SPI0_TXDATA) & METAL_SPI_TXDATA_FULL)
            ;

        /* Transfer byte by modifying the least significant byte in the TXDATA
         * register */
        if (tx_buf) {
            METAL_SPI_REGB(METAL_SIFIVE_SPI0_TXDATA) = tx_buf[i];
        } else {
            /* Transfer a 0 byte if the sending buffer is NULL */
            METAL_SPI_REGB(METAL_SIFIVE_SPI0_TXDATA) = 0;
        }

        /* Master receives bytes from the RX FIFO */

        /* Wait for RXFIFO to not be empty, but break the nested loops if
         * timeout this timeout method  needs refining, preferably taking into
         * account the device specs */
        endwait = metal_time() + METAL_SPI_RXDATA_TIMEOUT;

        while ((rxdata = METAL_SPI_REGW(METAL_SIFIVE_SPI0_RXDATA)) &
               METAL_SPI_RXDATA_EMPTY) {
            if (metal_time() > endwait) {
                /* If timeout, deassert the CS */
                METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSMODE) &=
                    ~(METAL_SPI_CSMODE_MASK);

                /* If timeout, return error code 1 immediately */
                return 1;
            }
        }

        /* Only store the dequeued byte if the receive_buffer is not NULL */
        if (rx_buf) {
            rx_buf[i] = (char)(rxdata & METAL_SPI_TXRXDATA_MASK);
        }
    }

    /* On the last byte, set CSMODE to auto so that the chip select transitions
     * back to high The reason that CS pin is not deasserted after transmitting
     * out the byte buffer is timing. The code on the host side likely executes
     * faster than the ability of FIFO to send out bytes. After the host
     * iterates through the array, fifo is likely not cleared yet. If host
     * deasserts the CS pin immediately, the following bytes in the output FIFO
     * will not be sent consecutively.
     * There needs to be a better way to handle this. */
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_CSMODE) &= ~(METAL_SPI_CSMODE_MASK);

    return 0;
}

int sifive_spi0_get_baud_rate(struct metal_spi spi) {
    return spi_state[get_index(spi)].baud_rate;
}

int sifive_spi0_set_baud_rate(struct metal_spi spi, int baud_rate) {
    uintptr_t control_base = dt_spi_data[get_index(spi)].base_addr;
    struct metal_clock clock = dt_spi_data[get_index(spi)].clock;

    spi_state[get_index(spi)].baud_rate = baud_rate;

    long clock_rate = metal_clock_get_rate_hz(clock);

    /* Calculate divider */
    long div = (clock_rate / (2 * baud_rate)) - 1;

    if (div > METAL_SPI_SCKDIV_MASK) {
        /* The requested baud rate is lower than we can support at
         * the current clock rate */
        return -1;
    }

    /* Set divider */
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_SCKDIV) &= ~METAL_SPI_SCKDIV_MASK;
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_SCKDIV) |= (div & METAL_SPI_SCKDIV_MASK);

    return 0;
}

void _sifive_spi0_pre_rate_change_callback(uint32_t id) {
    if (!spi_state[id].baud_rate)
        return;

    struct metal_spi spi = (struct metal_spi){id};
    uintptr_t control_base = dt_spi_data[get_index(spi)].base_addr;

    /* Detect when the TXDATA is empty by setting the transmit watermark count
     * to one and waiting until an interrupt is pending (indicating an empty
     * TXFIFO) */
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_TXMARK) &= ~(METAL_SPI_TXMARK_MASK);
    METAL_SPI_REGW(METAL_SIFIVE_SPI0_TXMARK) |= (METAL_SPI_TXMARK_MASK & 1);

    while ((METAL_SPI_REGW(METAL_SIFIVE_SPI0_IP) & METAL_SPI_TXWM) == 0)
        ;
}

void _sifive_spi0_post_rate_change_callback(uint32_t id) {
    if (!spi_state[id].baud_rate)
        return;

    struct metal_spi spi = (struct metal_spi){id};
    uint32_t baud_rate = spi_state[get_index(spi)].baud_rate;
    sifive_spi0_set_baud_rate(spi, baud_rate);
}

void sifive_spi0_init(struct metal_spi spi, int baud_rate) {
    uint32_t index = get_index(spi);

    sifive_spi0_set_baud_rate(spi, baud_rate);

    int has_pinmux = dt_spi_data[index].has_pinmux;
    if (has_pinmux) {
        struct metal_gpio pinmux = dt_spi_data[index].pinmux;
        uint32_t output_sel = dt_spi_data[index].pinmux_output_selector;
        uint32_t source_sel = dt_spi_data[index].pinmux_source_selector;
        metal_gpio_enable_pinmux(pinmux, output_sel, source_sel);
    }
}

#endif /* METAL_SIFIVE_SPI0 */

typedef int no_empty_translation_units;
