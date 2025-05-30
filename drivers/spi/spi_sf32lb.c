/*
 * Copyright (c) 2025, Qingsong Gou <gouqs@hotmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sf32lb_spi

#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <zephyr/sys_clock.h>
#include <zephyr/drivers/clock_control.h>

LOG_MODULE_REGISTER(spi_sf32lb, CONFIG_SPI_LOG_LEVEL);

/* SPI Register offsets */
#define SPI_CR1_OFFSET          0x00
#define SPI_CR2_OFFSET          0x04
#define SPI_SR_OFFSET           0x08
#define SPI_DR_OFFSET           0x0C
#define SPI_CRCPR_OFFSET        0x10
#define SPI_RXCRCR_OFFSET       0x14
#define SPI_TXCRCR_OFFSET       0x18
#define SPI_I2SCFGR_OFFSET      0x1C
#define SPI_I2SPR_OFFSET        0x20

/* SPI Control Register 1 bits */
#define SPI_CR1_CPHA            BIT(0)
#define SPI_CR1_CPOL            BIT(1)
#define SPI_CR1_MSTR            BIT(2)
#define SPI_CR1_BR_SHIFT        3
#define SPI_CR1_BR_MASK         0x7
#define SPI_CR1_SPE             BIT(6)
#define SPI_CR1_LSBFIRST        BIT(7)
#define SPI_CR1_SSI             BIT(8)
#define SPI_CR1_SSM             BIT(9)
#define SPI_CR1_RXONLY          BIT(10)
#define SPI_CR1_DFF             BIT(11)
#define SPI_CR1_CRCNEXT         BIT(12)
#define SPI_CR1_CRCEN           BIT(13)
#define SPI_CR1_BIDIOE          BIT(14)
#define SPI_CR1_BIDIMODE        BIT(15)

/* SPI Control Register 2 bits */
#define SPI_CR2_RXDMAEN         BIT(0)
#define SPI_CR2_TXDMAEN         BIT(1)
#define SPI_CR2_SSOE            BIT(2)
#define SPI_CR2_FRF             BIT(4)
#define SPI_CR2_ERRIE           BIT(5)
#define SPI_CR2_RXNEIE          BIT(6)
#define SPI_CR2_TXEIE           BIT(7)

/* SPI Status Register bits */
#define SPI_SR_RXNE             BIT(0)
#define SPI_SR_TXE              BIT(1)
#define SPI_SR_CHSIDE           BIT(2)
#define SPI_SR_UDR              BIT(3)
#define SPI_SR_CRCERR           BIT(4)
#define SPI_SR_MODF             BIT(5)
#define SPI_SR_OVR              BIT(6)
#define SPI_SR_BSY              BIT(7)
#define SPI_SR_FRE              BIT(8)

struct spi_sf32lb_config {
    uint32_t base;
    const struct device *clock_dev;
    const clock_control_subsys_t clock_subsys;
    uint32_t irq_num;
    void (*irq_config_func)(const struct device *dev);
};

struct spi_sf32lb_data {
    struct spi_context ctx;
    uint32_t word_size;
    uint32_t pclk_freq;
};

static int spi_sf32lb_configure(const struct device *dev,
                              const struct spi_config *config)
{
    const struct spi_sf32lb_config *cfg = dev->config;
    struct spi_sf32lb_data *data = dev->data;
    uint32_t reg_val;

    /* Get clock frequency */
    if (clock_control_get_rate(cfg->clock_dev, cfg->clock_subsys, &data->pclk_freq) != 0) {
        LOG_ERR("Failed to get clock frequency");
        return -EIO;
    }

    /* Calculate baud rate */
    uint32_t br = 0;
    uint32_t div = data->pclk_freq / config->frequency;
    while (div > (1 << (br + 1)) && br < 7) {
        br++;
    }

    /* Configure SPI */
    reg_val = SPI_CR1_SPE | SPI_CR1_MSTR;
    reg_val |= (br << SPI_CR1_BR_SHIFT) & SPI_CR1_BR_MASK;

    if (config->operation & SPI_MODE_CPOL) {
        reg_val |= SPI_CR1_CPOL;
    }
    if (config->operation & SPI_MODE_CPHA) {
        reg_val |= SPI_CR1_CPHA;
    }
    if (config->operation & SPI_MODE_LOOP) {
        reg_val |= SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;
    }
    if (config->operation & SPI_TRANSFER_LSB) {
        reg_val |= SPI_CR1_LSBFIRST;
    }

    /* Set word size */
    if (SPI_WORD_SIZE_GET(config->operation) == 8) {
        reg_val &= ~SPI_CR1_DFF;
        data->word_size = 1;
    } else {
        reg_val |= SPI_CR1_DFF;
        data->word_size = 2;
    }

    /* Write configuration */
    sys_write32(reg_val, cfg->base + SPI_CR1_OFFSET);

    /* Enable interrupts */
    reg_val = SPI_CR2_RXNEIE | SPI_CR2_ERRIE;
    sys_write32(reg_val, cfg->base + SPI_CR2_OFFSET);

    return 0;
}

static int spi_sf32lb_transceive(const struct device *dev,
                               const struct spi_config *config,
                               const struct spi_buf_set *tx_bufs,
                               const struct spi_buf_set *rx_bufs)
{
    const struct spi_sf32lb_config *cfg = dev->config;
    struct spi_sf32lb_data *data = dev->data;
    int ret;

    /* Configure SPI */
    ret = spi_sf32lb_configure(dev, config);
    if (ret != 0) {
        return ret;
    }

    /* Initialize context */
    spi_context_lock(&data->ctx, false, NULL, config);
    spi_context_buffers_setup(&data->ctx, tx_bufs, rx_bufs, data->word_size);

    /* Start transfer */
    while (spi_context_tx_buf_on(&data->ctx)) {
        /* Wait for TXE */
        while (!(sys_read32(cfg->base + SPI_SR_OFFSET) & SPI_SR_TXE)) {
            k_yield();
        }

        /* Write data */
        if (data->word_size == 1) {
            sys_write8(spi_context_tx_buf_get(&data->ctx),
                      cfg->base + SPI_DR_OFFSET);
        } else {
            sys_write16(spi_context_tx_buf_get(&data->ctx),
                       cfg->base + SPI_DR_OFFSET);
        }

        /* Wait for RXNE */
        while (!(sys_read32(cfg->base + SPI_SR_OFFSET) & SPI_SR_RXNE)) {
            k_yield();
        }

        /* Read data */
        if (data->word_size == 1) {
            spi_context_update_rx(&data->ctx, 1,
                                sys_read8(cfg->base + SPI_DR_OFFSET));
        } else {
            spi_context_update_rx(&data->ctx, 2,
                                sys_read16(cfg->base + SPI_DR_OFFSET));
        }
    }

    /* Wait for BSY to clear */
    while (sys_read32(cfg->base + SPI_SR_OFFSET) & SPI_SR_BSY) {
        k_yield();
    }

    spi_context_release(&data->ctx, ret);

    return ret;
}

static int spi_sf32lb_release(const struct device *dev,
                            const struct spi_config *config)
{
    const struct spi_sf32lb_config *cfg = dev->config;
    struct spi_sf32lb_data *data = dev->data;

    spi_context_unlock_unconditionally(&data->ctx);

    /* Disable SPI */
    sys_write32(0, cfg->base + SPI_CR1_OFFSET);

    return 0;
}

static const struct spi_driver_api spi_sf32lb_api = {
    .transceive = spi_sf32lb_transceive,
    .release = spi_sf32lb_release,
};

static void spi_sf32lb_isr(const struct device *dev)
{
    const struct spi_sf32lb_config *cfg = dev->config;
    struct spi_sf32lb_data *data = dev->data;
    uint32_t sr = sys_read32(cfg->base + SPI_SR_OFFSET);

    /* Handle errors */
    if (sr & (SPI_SR_OVR | SPI_SR_MODF | SPI_SR_CRCERR)) {
        LOG_ERR("SPI error: %x", sr);
        spi_context_complete(&data->ctx, dev, -EIO);
        return;
    }

    /* Handle RXNE */
    if (sr & SPI_SR_RXNE) {
        if (data->word_size == 1) {
            spi_context_update_rx(&data->ctx, 1,
                                sys_read8(cfg->base + SPI_DR_OFFSET));
        } else {
            spi_context_update_rx(&data->ctx, 2,
                                sys_read16(cfg->base + SPI_DR_OFFSET));
        }
    }

    /* Handle TXE */
    if (sr & SPI_SR_TXE) {
        if (spi_context_tx_buf_on(&data->ctx)) {
            if (data->word_size == 1) {
                sys_write8(spi_context_tx_buf_get(&data->ctx),
                          cfg->base + SPI_DR_OFFSET);
            } else {
                sys_write16(spi_context_tx_buf_get(&data->ctx),
                           cfg->base + SPI_DR_OFFSET);
            }
        } else {
            /* Disable TXE interrupt */
            sys_write32(SPI_CR2_RXNEIE | SPI_CR2_ERRIE,
                       cfg->base + SPI_CR2_OFFSET);
        }
    }
}

static int spi_sf32lb_init(const struct device *dev)
{
    const struct spi_sf32lb_config *cfg = dev->config;
    struct spi_sf32lb_data *data = dev->data;

    if (!device_is_ready(cfg->clock_dev)) {
        LOG_ERR("Clock control device not ready");
        return -ENODEV;
    }

    /* Enable clock */
    if (clock_control_on(cfg->clock_dev, cfg->clock_subsys) != 0) {
        LOG_ERR("Failed to enable clock");
        return -EIO;
    }

    /* Initialize context */
    spi_context_unlock_unconditionally(&data->ctx);

    /* Configure interrupt */
    cfg->irq_config_func(dev);

    return 0;
}

#define SPI_SF32LB_INIT(n) \
    static void spi_sf32lb_irq_config_func_##n(const struct device *dev); \
    static struct spi_sf32lb_data spi_sf32lb_data_##n; \
    static const struct spi_sf32lb_config spi_sf32lb_config_##n = { \
        .base = DT_INST_REG_ADDR(n), \
        .clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)), \
        .clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, id), \
        .irq_num = DT_INST_IRQN(n), \
        .irq_config_func = spi_sf32lb_irq_config_func_##n, \
    }; \
    static void spi_sf32lb_irq_config_func_##n(const struct device *dev) \
    { \
        IRQ_CONNECT(DT_INST_IRQN(n), \
                   DT_INST_IRQ(n, priority), \
                   spi_sf32lb_isr, \
                   DEVICE_DT_INST_GET(n), \
                   0); \
        irq_enable(DT_INST_IRQN(n)); \
    } \
    DEVICE_DT_INST_DEFINE(n, \
                         spi_sf32lb_init, \
                         NULL, \
                         &spi_sf32lb_data_##n, \
                         &spi_sf32lb_config_##n, \
                         POST_KERNEL, \
                         CONFIG_SPI_INIT_PRIORITY, \
                         &spi_sf32lb_api);

DT_INST_FOREACH_STATUS_OKAY(SPI_SF32LB_INIT) 