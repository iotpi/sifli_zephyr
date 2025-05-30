/*
 * Copyright (c) 2025, Qingsong Gou <gouqs@hotmail.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sifli_dma

#include <zephyr/kernel.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <soc.h>
#include <bf0_hal.h>

LOG_MODULE_REGISTER(dma_sf32lb, CONFIG_DMA_LOG_LEVEL);

struct dma_sf32lb_data {
    DMA_HandleTypeDef hdma;
    struct k_sem sem;
    dma_callback_t callback;
    void *user_data;
    uint32_t channel;
    bool busy;
};

struct dma_sf32lb_config {
    const struct pinctrl_dev_config *pcfg;
    const struct device *clock_dev;
    clock_control_subsys_t clock_subsys;
    uint32_t reg;
    uint32_t irq;
    void (*irq_config_func)(const struct device *dev);
};

static void dma_sf32lb_isr(const struct device *dev)
{
    const struct dma_sf32lb_config *config = dev->config;
    struct dma_sf32lb_data *data = dev->data;

    HAL_DMA_IRQHandler(&data->hdma);
}

static void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
    struct dma_sf32lb_data *data = CONTAINER_OF(hdma, struct dma_sf32lb_data, hdma);
    const struct device *dev = CONTAINER_OF(data, const struct device, data);

    if (data->callback) {
        data->callback(dev, data->user_data, 0, 0);
    }

    data->busy = false;
    k_sem_give(&data->sem);
}

static void HAL_DMA_XferErrorCallback(DMA_HandleTypeDef *hdma)
{
    struct dma_sf32lb_data *data = CONTAINER_OF(hdma, struct dma_sf32lb_data, hdma);
    const struct device *dev = CONTAINER_OF(data, const struct device, data);

    if (data->callback) {
        data->callback(dev, data->user_data, -EIO, 0);
    }

    data->busy = false;
    k_sem_give(&data->sem);
}

static int dma_sf32lb_configure(const struct device *dev, uint32_t channel,
                              struct dma_config *config)
{
    const struct dma_sf32lb_config *dma_config = dev->config;
    struct dma_sf32lb_data *data = dev->data;
    DMA_InitTypeDef dma_init = {0};
    int ret;

    if (channel >= 8) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    if (data->busy) {
        LOG_ERR("Channel %d is busy", channel);
        return -EBUSY;
    }

    data->channel = channel;
    data->callback = config->callback;
    data->user_data = config->user_data;

    /* Configure DMA */
    dma_init.Direction = DMA_MEMORY_TO_MEMORY;
    dma_init.PeriphInc = DMA_PINC_ENABLE;
    dma_init.MemInc = DMA_MINC_ENABLE;
    dma_init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    dma_init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    dma_init.Mode = DMA_NORMAL;
    dma_init.Priority = DMA_PRIORITY_HIGH;

    if (config->block_count > 1) {
        dma_init.Mode = DMA_CIRCULAR;
    }

    switch (config->source_data_size) {
    case 1:
        dma_init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        break;
    case 2:
        dma_init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        break;
    case 4:
        dma_init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        break;
    default:
        LOG_ERR("Invalid source data size %d", config->source_data_size);
        return -EINVAL;
    }

    switch (config->dest_data_size) {
    case 1:
        dma_init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        break;
    case 2:
        dma_init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        break;
    case 4:
        dma_init.MemDataAlignment = DMA_MDATAALIGN_WORD;
        break;
    default:
        LOG_ERR("Invalid destination data size %d", config->dest_data_size);
        return -EINVAL;
    }

    data->hdma.Instance = (DMA_TypeDef *)dma_config->reg;
    data->hdma.Init = dma_init;

    ret = HAL_DMA_Init(&data->hdma);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to initialize DMA");
        return -EIO;
    }

    return 0;
}

static int dma_sf32lb_reload(const struct device *dev, uint32_t channel,
                            uint32_t src, uint32_t dst, size_t size)
{
    const struct dma_sf32lb_config *config = dev->config;
    struct dma_sf32lb_data *data = dev->data;
    int ret;

    if (channel != data->channel) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    if (data->busy) {
        LOG_ERR("Channel %d is busy", channel);
        return -EBUSY;
    }

    data->busy = true;

    ret = HAL_DMA_Start_IT(&data->hdma, src, dst, size);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to start DMA transfer");
        data->busy = false;
        return -EIO;
    }

    return 0;
}

static int dma_sf32lb_start(const struct device *dev, uint32_t channel)
{
    const struct dma_sf32lb_config *config = dev->config;
    struct dma_sf32lb_data *data = dev->data;

    if (channel != data->channel) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    if (data->busy) {
        LOG_ERR("Channel %d is busy", channel);
        return -EBUSY;
    }

    return 0;
}

static int dma_sf32lb_stop(const struct device *dev, uint32_t channel)
{
    const struct dma_sf32lb_config *config = dev->config;
    struct dma_sf32lb_data *data = dev->data;
    int ret;

    if (channel != data->channel) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    if (!data->busy) {
        return 0;
    }

    ret = HAL_DMA_Abort(&data->hdma);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to stop DMA transfer");
        return -EIO;
    }

    data->busy = false;
    return 0;
}

static int dma_sf32lb_suspend(const struct device *dev, uint32_t channel)
{
    const struct dma_sf32lb_config *config = dev->config;
    struct dma_sf32lb_data *data = dev->data;
    int ret;

    if (channel != data->channel) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    if (!data->busy) {
        return 0;
    }

    ret = HAL_DMA_PollForTransfer(&data->hdma, HAL_DMA_FULL_TRANSFER, 1000);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to suspend DMA transfer");
        return -EIO;
    }

    return 0;
}

static int dma_sf32lb_resume(const struct device *dev, uint32_t channel)
{
    const struct dma_sf32lb_config *config = dev->config;
    struct dma_sf32lb_data *data = dev->data;

    if (channel != data->channel) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    if (!data->busy) {
        return 0;
    }

    return 0;
}

static int dma_sf32lb_get_status(const struct device *dev, uint32_t channel,
                                struct dma_status *status)
{
    const struct dma_sf32lb_config *config = dev->config;
    struct dma_sf32lb_data *data = dev->data;

    if (channel != data->channel) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    status->busy = data->busy;
    status->pending_length = 0;
    status->free = !data->busy;

    return 0;
}

static int dma_sf32lb_init(const struct device *dev)
{
    const struct dma_sf32lb_config *config = dev->config;
    struct dma_sf32lb_data *data = dev->data;
    int ret;

    /* Configure pins */
    ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
    if (ret < 0) {
        LOG_ERR("Failed to configure pins");
        return ret;
    }

    /* Enable clock */
    ret = clock_control_on(config->clock_dev, config->clock_subsys);
    if (ret < 0) {
        LOG_ERR("Failed to enable clock");
        return ret;
    }

    /* Initialize semaphore */
    k_sem_init(&data->sem, 1, 1);

    /* Configure interrupt */
    config->irq_config_func(dev);

    return 0;
}

static const struct dma_driver_api dma_sf32lb_driver_api = {
    .configure = dma_sf32lb_configure,
    .reload = dma_sf32lb_reload,
    .start = dma_sf32lb_start,
    .stop = dma_sf32lb_stop,
    .suspend = dma_sf32lb_suspend,
    .resume = dma_sf32lb_resume,
    .get_status = dma_sf32lb_get_status,
};

#define DMA_SF32LB_INIT(n) \
    static void dma_sf32lb_irq_config_func_##n(const struct device *dev); \
    static struct dma_sf32lb_data dma_sf32lb_data_##n; \
    PINCTRL_DT_INST_DEFINE(n); \
    static const struct dma_sf32lb_config dma_sf32lb_config_##n = { \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n), \
        .clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)), \
        .clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name), \
        .reg = DT_INST_REG_ADDR(n), \
        .irq = DT_INST_IRQN(n), \
        .irq_config_func = dma_sf32lb_irq_config_func_##n, \
    }; \
    DEVICE_DT_INST_DEFINE(n, \
        dma_sf32lb_init, \
        NULL, \
        &dma_sf32lb_data_##n, \
        &dma_sf32lb_config_##n, \
        POST_KERNEL, \
        CONFIG_DMA_INIT_PRIORITY, \
        &dma_sf32lb_driver_api); \
    static void dma_sf32lb_irq_config_func_##n(const struct device *dev) \
    { \
        IRQ_CONNECT(DT_INST_IRQN(n), \
            DT_INST_IRQ(n, priority), \
            dma_sf32lb_isr, \
            DEVICE_DT_INST_GET(n), \
            0); \
        irq_enable(DT_INST_IRQN(n)); \
    }

DT_INST_FOREACH_STATUS_OKAY(DMA_SF32LB_INIT) 