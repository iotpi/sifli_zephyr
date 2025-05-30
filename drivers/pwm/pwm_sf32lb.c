/*
 * Copyright (c) 2025, Qingsong Gou <gouqs@hotmail.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sifli_pwm

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <soc.h>
#include <bf0_hal.h>

LOG_MODULE_REGISTER(pwm_sf32lb, CONFIG_PWM_LOG_LEVEL);

struct pwm_sf32lb_data {
    TIM_HandleTypeDef htim;
    uint32_t period_cycles;
    uint32_t clock_freq;
    uint32_t capture_rising;
    uint32_t capture_falling;
    bool capture_done;
    struct k_sem capture_sem;
};

struct pwm_sf32lb_config {
    const struct pinctrl_dev_config *pcfg;
    const struct device *clock_dev;
    clock_control_subsys_t clock_subsys;
    uint32_t reg;
    uint32_t irq;
    void (*irq_config_func)(const struct device *dev);
    uint32_t pwm_channel;
    bool is_capture;
};

static void pwm_sf32lb_capture_callback(TIM_HandleTypeDef *htim)
{
    const struct device *dev = (const struct device *)htim->Instance;
    struct pwm_sf32lb_data *data = dev->data;

    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        if (htim->Instance->CCMR1 & TIM_CCMR1_CC1S_Msk) {
            /* Rising edge detected */
            data->capture_rising = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            /* Switch to falling edge */
            htim->Instance->CCER &= ~TIM_CCER_CC1P;
        } else {
            /* Falling edge detected */
            data->capture_falling = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            /* Switch to rising edge */
            htim->Instance->CCER |= TIM_CCER_CC1P;
            data->capture_done = true;
            k_sem_give(&data->capture_sem);
        }
    }
}

static int pwm_sf32lb_set_cycles(const struct device *dev, uint32_t channel,
                                uint32_t period_cycles, uint32_t pulse_cycles,
                                pwm_flags_t flags)
{
    const struct pwm_sf32lb_config *config = dev->config;
    struct pwm_sf32lb_data *data = dev->data;
    TIM_OC_InitTypeDef sConfigOC = {0};
    int ret;

    if (config->is_capture) {
        LOG_ERR("Device configured for capture mode");
        return -ENOTSUP;
    }

    if (channel != config->pwm_channel) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    if (period_cycles == 0) {
        LOG_ERR("Invalid period cycles");
        return -EINVAL;
    }

    if (pulse_cycles > period_cycles) {
        LOG_ERR("Pulse cycles %d > period cycles %d", pulse_cycles, period_cycles);
        return -EINVAL;
    }

    if (flags & PWM_POLARITY_INVERTED) {
        LOG_ERR("Inverted polarity not supported");
        return -ENOTSUP;
    }

    /* Configure PWM */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = pulse_cycles;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    ret = HAL_TIM_PWM_ConfigChannel(&data->htim, &sConfigOC, TIM_CHANNEL_1);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to configure PWM channel");
        return -EIO;
    }

    /* Update period */
    data->htim.Init.Period = period_cycles - 1;
    ret = HAL_TIM_Base_Init(&data->htim);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to update period");
        return -EIO;
    }

    data->period_cycles = period_cycles;

    return 0;
}

static int pwm_sf32lb_get_cycles_per_sec(const struct device *dev, uint32_t channel,
                                        uint64_t *cycles)
{
    const struct pwm_sf32lb_config *config = dev->config;
    struct pwm_sf32lb_data *data = dev->data;

    if (channel != config->pwm_channel) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    *cycles = data->clock_freq;
    return 0;
}

static int pwm_sf32lb_capture_cycles(const struct device *dev, uint32_t channel,
                                   uint32_t *period_cycles, uint32_t *pulse_cycles,
                                   pwm_flags_t flags)
{
    const struct pwm_sf32lb_config *config = dev->config;
    struct pwm_sf32lb_data *data = dev->data;
    int ret;

    if (!config->is_capture) {
        LOG_ERR("Device not configured for capture mode");
        return -ENOTSUP;
    }

    if (channel != config->pwm_channel) {
        LOG_ERR("Invalid channel %d", channel);
        return -EINVAL;
    }

    if (flags & PWM_CAPTURE_TYPE_PERIOD) {
        /* Start capture */
        data->capture_done = false;
        ret = HAL_TIM_IC_Start_IT(&data->htim, TIM_CHANNEL_1);
        if (ret != HAL_OK) {
            LOG_ERR("Failed to start capture");
            return -EIO;
        }

        /* Wait for capture to complete */
        ret = k_sem_take(&data->capture_sem, K_MSEC(1000));
        if (ret != 0) {
            LOG_ERR("Capture timeout");
            return -ETIMEDOUT;
        }

        /* Calculate period and pulse width */
        if (data->capture_falling > data->capture_rising) {
            *period_cycles = data->capture_falling - data->capture_rising;
        } else {
            *period_cycles = (0xFFFF - data->capture_rising) + data->capture_falling;
        }
        *pulse_cycles = *period_cycles / 2; /* Assuming 50% duty cycle */
    }

    return 0;
}

static int pwm_sf32lb_init(const struct device *dev)
{
    const struct pwm_sf32lb_config *config = dev->config;
    struct pwm_sf32lb_data *data = dev->data;
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_IC_InitTypeDef sConfigIC = {0};
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

    /* Get clock frequency */
    ret = clock_control_get_rate(config->clock_dev, config->clock_subsys, &data->clock_freq);
    if (ret < 0) {
        LOG_ERR("Failed to get clock frequency");
        return ret;
    }

    /* Initialize timer */
    data->htim.Instance = (TIM_TypeDef *)config->reg;
    data->htim.Init.Prescaler = 0;
    data->htim.Init.CounterMode = TIM_COUNTERMODE_UP;
    data->htim.Init.Period = 0xFFFF;
    data->htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    data->htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    ret = HAL_TIM_Base_Init(&data->htim);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to initialize timer");
        return -EIO;
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    ret = HAL_TIM_ConfigClockSource(&data->htim, &sClockSourceConfig);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to configure clock source");
        return -EIO;
    }

    if (config->is_capture) {
        /* Configure input capture */
        sConfigIC.ICPolarity = TIM_ICPOLARITY_RISING;
        sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
        sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
        sConfigIC.ICFilter = 0;

        ret = HAL_TIM_IC_ConfigChannel(&data->htim, &sConfigIC, TIM_CHANNEL_1);
        if (ret != HAL_OK) {
            LOG_ERR("Failed to configure input capture");
            return -EIO;
        }

        /* Initialize capture semaphore */
        k_sem_init(&data->capture_sem, 0, 1);
    } else {
        ret = HAL_TIM_PWM_Init(&data->htim);
        if (ret != HAL_OK) {
            LOG_ERR("Failed to initialize PWM");
            return -EIO;
        }
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    ret = HAL_TIMEx_MasterConfigSynchronization(&data->htim, &sMasterConfig);
    if (ret != HAL_OK) {
        LOG_ERR("Failed to configure master/slave mode");
        return -EIO;
    }

    /* Configure interrupt */
    config->irq_config_func(dev);

    if (!config->is_capture) {
        /* Start PWM */
        ret = HAL_TIM_PWM_Start(&data->htim, TIM_CHANNEL_1);
        if (ret != HAL_OK) {
            LOG_ERR("Failed to start PWM");
            return -EIO;
        }
    }

    return 0;
}

static const struct pwm_driver_api pwm_sf32lb_driver_api = {
    .set_cycles = pwm_sf32lb_set_cycles,
    .get_cycles_per_sec = pwm_sf32lb_get_cycles_per_sec,
    .capture_cycles = pwm_sf32lb_capture_cycles,
};

#define PWM_SF32LB_INIT(n) \
    static void pwm_sf32lb_irq_config_func_##n(const struct device *dev); \
    static struct pwm_sf32lb_data pwm_sf32lb_data_##n; \
    PINCTRL_DT_INST_DEFINE(n); \
    static const struct pwm_sf32lb_config pwm_sf32lb_config_##n = { \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n), \
        .clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)), \
        .clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name), \
        .reg = DT_INST_REG_ADDR(n), \
        .irq = DT_INST_IRQN(n), \
        .irq_config_func = pwm_sf32lb_irq_config_func_##n, \
        .pwm_channel = DT_INST_PROP(n, pwm_channel), \
        .is_capture = DT_INST_PROP(n, is_capture), \
    }; \
    DEVICE_DT_INST_DEFINE(n, \
        pwm_sf32lb_init, \
        NULL, \
        &pwm_sf32lb_data_##n, \
        &pwm_sf32lb_config_##n, \
        POST_KERNEL, \
        CONFIG_PWM_INIT_PRIORITY, \
        &pwm_sf32lb_driver_api); \
    static void pwm_sf32lb_irq_config_func_##n(const struct device *dev) \
    { \
        IRQ_CONNECT(DT_INST_IRQN(n), \
            DT_INST_IRQ(n, priority), \
            HAL_TIM_IRQHandler, \
            DEVICE_DT_INST_GET(n), \
            0); \
        irq_enable(DT_INST_IRQN(n)); \
    }

DT_INST_FOREACH_STATUS_OKAY(PWM_SF32LB_INIT)