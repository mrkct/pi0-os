#pragma once

#include <kernel/base.h>


struct BCM2835AuxRegisterMap {
    uint32_t irq;
    uint32_t enables;
    uint8_t reserved[0x40 - 0x4 - 4];
    
    // Mini UART
    uint32_t mu_io_reg;
    uint32_t mu_ier_reg;
    uint32_t mu_iir_reg;
    uint32_t mu_lcr_reg;
    uint32_t mu_mcr_reg;
    uint32_t mu_lsr_reg;
    uint32_t mu_msr_reg;
    uint32_t mu_scratch;
    uint32_t mu_cntl_reg;
    uint32_t mu_stat_reg;
    uint32_t mu_baud_reg;

    uint8_t reserved2[0x80 - 0x68 - 4];
    // SPI 1
    uint32_t spi0_cntl0_reg;
    uint32_t spi0_cntl1_reg;
    uint32_t spi0_stat_reg;
    uint8_t reserved3[0x90 - 0x88 - 4];
    uint32_t spi0_io_reg;
    uint32_t spi0_peek_reg;

    uint8_t reserved4[0xc0 - 0x94 - 4];
    // SPI 2
    uint32_t spi1_cntl0_reg;
    uint32_t spi1_cntl1_reg;
    uint32_t spi1_stat_reg;
    uint8_t reserved5[0xd0 - 0xc8 - 4];
    uint32_t spi1_io_reg;
    uint32_t spi1_peek_reg;
};
static_assert(sizeof(BCM2835AuxRegisterMap) == 0xd8);

static_assert(offsetof(BCM2835AuxRegisterMap, mu_io_reg) == 0x40);
static_assert(offsetof(BCM2835AuxRegisterMap, spi0_cntl0_reg) == 0x80);
static_assert(offsetof(BCM2835AuxRegisterMap, spi0_io_reg) == 0x90);
static_assert(offsetof(BCM2835AuxRegisterMap, spi1_cntl0_reg) == 0xc0);
