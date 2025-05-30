/*
 * Copyright (c) 2025, Qingsong Gou <gouqs@hotmail.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sifli_counter

#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <soc.h>
#include <bf0_hal.h>

LOG_MODULE_REGISTER(counter_sf32lb, CONFIG_COUNTER_LOG_LEVEL);

struct counter_sf32lb_data {
    LPTIM_HandleTypeDef hlptim;
    struct k_sem sem;
    counter_alarm_callback_t alarm_callback;
    void *alarm_user_data;
    counter_top_callback_t top_callback;
    void *top_user_data;
    uint32_t top_value;
    bool alarm_active;
};

struct counter_sf32lb_config {
    const struct pinctrl_dev_config *pcfg;
    const struct device *clock_dev;
    clock_control_subsys_t clock_subsys;
    uint32_t reg;
    uint32_t irq;
    void (*irq_config_func)(const struct device *dev);
};

static void counter_sf32lb_isr(const struct device *dev)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;

    HAL_LPTIM_IRQHandler(&data->hlptim);
}

static void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
    struct counter_sf32lb_data *data = CONTAINER_OF(hlptim, struct counter_sf32lb_data, hlptim);
    const struct device *dev = CONTAINER_OF(data, const struct device, data);

    if (data->top_callback) {
        data->top_callback(dev, data->top_user_data);
    }
}

static void HAL_LPTIM_CompareMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
    struct counter_sf32lb_data *data = CONTAINER_OF(hlptim, struct counter_sf32lb_data, hlptim);
    const struct device *dev = CONTAINER_OF(data, const struct device, data);

    if (data->alarm_callback) {
        data->alarm_callback(dev, 0, 0, data->alarm_user_data);
        data->alarm_active = false;
    }
}

static int counter_sf32lb_start(const struct device *dev)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;
    int ret;

    ret = HAL_LPTIM_Counter_Start(&data->hlptim, data->top_value);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to start counter");
        return -EIO;
    }

    return 0;
}

static int counter_sf32lb_stop(const struct device *dev)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;
    int ret;

    ret = HAL_LPTIM_Counter_Stop(&data->hlptim);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to stop counter");
        return -EIO;
    }

    return 0;
}

static int counter_sf32lb_get_value(const struct device *dev, uint32_t *ticks)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;

    *ticks = HAL_LPTIM_ReadCounter(&data->hlptim);
    return 0;
}

static int counter_sf32lb_set_alarm(const struct device *dev, uint8_t chan_id,
                                  const struct counter_alarm_cfg *alarm_cfg)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;
    int ret;

    if (chan_id != 0) {
        LOG_ERR("Invalid channel %d", chan_id);
        return -EINVAL;
    }

    if (data->alarm_active) {
        LOG_ERR("Alarm already active");
        return -EBUSY;
    }

    data->alarm_callback = alarm_cfg->callback;
    data->alarm_user_data = alarm_cfg->user_data;

    ret = HAL_LPTIM_TimeOut_Start_IT(&data->hlptim, alarm_cfg->ticks);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to set alarm");
        return -EIO;
    }

    data->alarm_active = true;
    return 0;
}

static int counter_sf32lb_cancel_alarm(const struct device *dev, uint8_t chan_id)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;
    int ret;

    if (chan_id != 0) {
        LOG_ERR("Invalid channel %d", chan_id);
        return -EINVAL;
    }

    if (!data->alarm_active) {
        return 0;
    }

    ret = HAL_LPTIM_TimeOut_Stop_IT(&data->hlptim);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to cancel alarm");
        return -EIO;
    }

    data->alarm_active = false;
    return 0;
}

static int counter_sf32lb_set_top_value(const struct device *dev,
                                      const struct counter_top_cfg *top_cfg)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;
    int ret;

    if (top_cfg->flags & COUNTER_TOP_CFG_DONT_RESET) {
        LOG_ERR("DONT_RESET flag not supported");
        return -ENOTSUP;
    }

    data->top_value = top_cfg->ticks;
    data->top_callback = top_cfg->callback;
    data->top_user_data = top_cfg->user_data;

    ret = HAL_LPTIM_Counter_Start(&data->hlptim, data->top_value);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to set top value");
        return -EIO;
    }

    return 0;
}

static uint32_t counter_sf32lb_get_top_value(const struct device *dev)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;

    return data->top_value;
}

static uint32_t counter_sf32lb_get_frequency(const struct device *dev)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;

    return HAL_RCC_GetPCLK1Freq() / 4;
}

static int counter_sf32lb_init(const struct device *dev)
{
    const struct counter_sf32lb_config *config = dev->config;
    struct counter_sf32lb_data *data = dev->data;
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

    /* Initialize LPTIM */
    data->hlptim.Instance = (LPTIM_TypeDef *)config->reg;
    data->hlptim.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
    data->hlptim.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV4;
    data->hlptim.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
    data->hlptim.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
    data->hlptim.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
    data->hlptim.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
    data->hlptim.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
    data->hlptim.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;

    ret = HAL_LPTIM_Init(&data->hlptim);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to initialize LPTIM");
        return -EIO;
    }

    /* Initialize semaphore */
    k_sem_init(&data->sem, 0, 1);

    /* Configure interrupt */
    config->irq_config_func(dev);

    return 0;
}

static const struct counter_driver_api counter_sf32lb_driver_api = {
    .start = counter_sf32lb_start,
    .stop = counter_sf32lb_stop,
    .get_value = counter_sf32lb_get_value,
    .set_alarm = counter_sf32lb_set_alarm,
    .cancel_alarm = counter_sf32lb_cancel_alarm,
    .set_top_value = counter_sf32lb_set_top_value,
    .get_top_value = counter_sf32lb_get_top_value,
    .get_freq = counter_sf32lb_get_frequency,
};

#define COUNTER_SF32LB_INIT(n) \
    static void counter_sf32lb_irq_config_func_##n(const struct device *dev); \
    static struct counter_sf32lb_data counter_sf32lb_data_##n; \
    PINCTRL_DT_INST_DEFINE(n); \
    static const struct counter_sf32lb_config counter_sf32lb_config_##n = { \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n), \
        .clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)), \
        .clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name), \
        .reg = DT_INST_REG_ADDR(n), \
        .irq = DT_INST_IRQN(n), \
        .irq_config_func = counter_sf32lb_irq_config_func_##n, \
    }; \
    DEVICE_DT_INST_DEFINE(n, \
        counter_sf32lb_init, \
        NULL, \
        &counter_sf32lb_data_##n, \
        &counter_sf32lb_config_##n, \
        POST_KERNEL, \
        CONFIG_COUNTER_INIT_PRIORITY, \
        &counter_sf32lb_driver_api); \
    static void counter_sf32lb_irq_config_func_##n(const struct device *dev) \
    { \
        IRQ_CONNECT(DT_INST_IRQN(n), \
            DT_INST_IRQ(n, priority), \
            counter_sf32lb_isr, \
            DEVICE_DT_INST_GET(n), \
            0); \
        irq_enable(DT_INST_IRQN(n)); \
    }

DT_INST_FOREACH_STATUS_OKAY(COUNTER_SF32LB_INIT) 