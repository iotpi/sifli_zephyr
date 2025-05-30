/*
 * Copyright (c) 2025, Qingsong Gou <gouqs@hotmail.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sifli_adc

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <soc.h>
#include <bf0_hal.h>

LOG_MODULE_REGISTER(adc_sf32lb, CONFIG_ADC_LOG_LEVEL);

struct adc_sf32lb_data {
    ADC_HandleTypeDef hadc;
    struct k_sem sem;
    uint16_t *buffer;
    uint8_t active_channels;
    bool continuous;
    adc_sequence_callback_t callback;
    void *user_data;
};

struct adc_sf32lb_config {
    const struct pinctrl_dev_config *pcfg;
    const struct device *clock_dev;
    clock_control_subsys_t clock_subsys;
    uint32_t reg;
    uint32_t irq;
    void (*irq_config_func)(const struct device *dev);
};

static void adc_sf32lb_isr(const struct device *dev)
{
    const struct adc_sf32lb_config *config = dev->config;
    struct adc_sf32lb_data *data = dev->data;

    HAL_ADC_IRQHandler(&data->hadc);
}

static void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    struct adc_sf32lb_data *data = CONTAINER_OF(hadc, struct adc_sf32lb_data, hadc);
    const struct device *dev = CONTAINER_OF(data, const struct device, data);

    if (data->callback) {
        data->callback(dev, data->buffer, data->active_channels, data->user_data);
    }

    if (!data->continuous) {
        k_sem_give(&data->sem);
    }
}

static int adc_sf32lb_channel_setup(const struct device *dev,
                                  const struct adc_channel_cfg *channel_cfg)
{
    const struct adc_sf32lb_config *config = dev->config;
    struct adc_sf32lb_data *data = dev->data;
    ADC_ChannelConfTypeDef sConfig = {0};
    int ret;

    if (channel_cfg->channel_id >= 16) {
        LOG_ERR("Channel %d is not valid", channel_cfg->channel_id);
        return -EINVAL;
    }

    if (channel_cfg->acquisition_time != ADC_ACQ_TIME_DEFAULT) {
        LOG_ERR("Acquisition time is not supported");
        return -ENOTSUP;
    }

    if (channel_cfg->differential) {
        LOG_ERR("Differential channels are not supported");
        return -ENOTSUP;
    }

    if (channel_cfg->gain != ADC_GAIN_1) {
        LOG_ERR("Gain is not supported");
        return -ENOTSUP;
    }

    if (channel_cfg->reference != ADC_REF_INTERNAL) {
        LOG_ERR("External reference is not supported");
        return -ENOTSUP;
    }

    /* Configure ADC channel */
    sConfig.Channel = channel_cfg->channel_id;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;

    ret = HAL_ADC_ConfigChannel(&data->hadc, &sConfig);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to configure ADC channel %d", channel_cfg->channel_id);
        return -EIO;
    }

    return 0;
}

static int adc_sf32lb_read(const struct device *dev,
                          const struct adc_sequence *sequence)
{
    const struct adc_sf32lb_config *config = dev->config;
    struct adc_sf32lb_data *data = dev->data;
    int ret;

    if (sequence->resolution != 12) {
        LOG_ERR("Resolution %d is not supported", sequence->resolution);
        return -ENOTSUP;
    }

    if (sequence->oversampling) {
        LOG_ERR("Oversampling is not supported");
        return -ENOTSUP;
    }

    if (sequence->calibrate) {
        LOG_ERR("Calibration is not supported");
        return -ENOTSUP;
    }

    data->buffer = sequence->buffer;
    data->active_channels = sequence->channels;
    data->continuous = (sequence->options & ADC_OPT_CONTINUOUS) != 0;
    data->callback = sequence->callback;
    data->user_data = sequence->user_data;

    if (data->continuous) {
        ret = HAL_ADC_Start_DMA(&data->hadc, (uint32_t *)data->buffer,
                               sequence->buffer_size);
    } else {
        ret = HAL_ADC_Start_IT(&data->hadc);
    }

    if (ret != HAL_OK) {
        LOG_ERR("Failed to start ADC conversion");
        return -EIO;
    }

    if (!data->continuous) {
        ret = k_sem_take(&data->sem, K_MSEC(1000));
        if (ret != 0) {
            LOG_ERR("ADC conversion timeout");
            return -ETIMEDOUT;
        }
    }

    return 0;
}

static int adc_sf32lb_init(const struct device *dev)
{
    const struct adc_sf32lb_config *config = dev->config;
    struct adc_sf32lb_data *data = dev->data;
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

    /* Initialize ADC */
    data->hadc.Instance = (ADC_TypeDef *)config->reg;
    data->hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    data->hadc.Init.Resolution = ADC_RESOLUTION_12B;
    data->hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    data->hadc.Init.ScanConvMode = ADC_SCAN_ENABLE;
    data->hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    data->hadc.Init.LowPowerAutoWait = DISABLE;
    data->hadc.Init.LowPowerAutoPowerOff = DISABLE;
    data->hadc.Init.ContinuousConvMode = DISABLE;
    data->hadc.Init.NbrOfConversion = 1;
    data->hadc.Init.DiscontinuousConvMode = DISABLE;
    data->hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    data->hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    data->hadc.Init.DMAContinuousRequests = DISABLE;
    data->hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    data->hadc.Init.OversamplingMode = DISABLE;

    ret = HAL_ADC_Init(&data->hadc);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to initialize ADC");
        return -EIO;
    }

    /* Initialize semaphore */
    k_sem_init(&data->sem, 0, 1);

    /* Configure interrupt */
    config->irq_config_func(dev);

    return 0;
}

static const struct adc_driver_api adc_sf32lb_driver_api = {
    .channel_setup = adc_sf32lb_channel_setup,
    .read = adc_sf32lb_read,
};

#define ADC_SF32LB_INIT(n) \
    static void adc_sf32lb_irq_config_func_##n(const struct device *dev); \
    static struct adc_sf32lb_data adc_sf32lb_data_##n; \
    PINCTRL_DT_INST_DEFINE(n); \
    static const struct adc_sf32lb_config adc_sf32lb_config_##n = { \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n), \
        .clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)), \
        .clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name), \
        .reg = DT_INST_REG_ADDR(n), \
        .irq = DT_INST_IRQN(n), \
        .irq_config_func = adc_sf32lb_irq_config_func_##n, \
    }; \
    DEVICE_DT_INST_DEFINE(n, \
        adc_sf32lb_init, \
        NULL, \
        &adc_sf32lb_data_##n, \
        &adc_sf32lb_config_##n, \
        POST_KERNEL, \
        CONFIG_ADC_INIT_PRIORITY, \
        &adc_sf32lb_driver_api); \
    static void adc_sf32lb_irq_config_func_##n(const struct device *dev) \
    { \
        IRQ_CONNECT(DT_INST_IRQN(n), \
            DT_INST_IRQ(n, priority), \
            adc_sf32lb_isr, \
            DEVICE_DT_INST_GET(n), \
            0); \
        irq_enable(DT_INST_IRQN(n)); \
    }

DT_INST_FOREACH_STATUS_OKAY(ADC_SF32LB_INIT)
