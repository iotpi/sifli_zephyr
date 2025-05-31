/*
 * Copyright (c) 2025 Qingsong Gou <gouqs@hotmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sifli_rcc

#include <stdint.h>

#include <zephyr/arch/cpu.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/clock_control/sf32lb_clock_control.h>

LOG_MODULE_REGISTER(clock_sf32lb, CONFIG_CLOCK_CONTROL_LOG_LEVEL);

struct sf32lb_clock_config {
};

/* Clock control device data */
struct sf32lb_clock_data {
	uint32_t sys_clk_freq;
	uint32_t ahb_clk_freq;
	uint32_t apb1_clk_freq;
	uint32_t apb2_clk_freq;
};

/* Clock control device API */
static int sf32lb_clock_control_on(const struct device *dev,
				 clock_control_subsys_t subsys)
{
    // const struct sf32lb_clock_config *config = dev->config;
    ARG_UNUSED(dev);
    struct sf32lb_rcc_clock * rcc = (struct sf32lb_rcc_clock * )subsys;
	sys_set_bit(DT_REG_ADDR(DT_NODELABEL(rcc)) + rcc->reg, rcc->bit);

	return 0;
}

static int sf32lb_clock_control_off(const struct device *dev,
				  clock_control_subsys_t subsys)
{
    ARG_UNUSED(dev);
    struct sf32lb_rcc_clock * rcc = (struct sf32lb_rcc_clock * )subsys;
	sys_clear_bit(DT_REG_ADDR(DT_NODELABEL(rcc)) + rcc->reg, rcc->bit);

	return 0;
}

static int sf32lb_clock_control_get_rate(const struct device *dev,
					clock_control_subsys_t subsys,
					uint32_t *rate)
{
	RCC_MODULE_TYPE module = (RCC_MODULE_TYPE)subsys;

	*rate = HAL_RCC_GetModuleFreq(module);

	return 0;
}

static const struct clock_control_driver_api sf32lb_clock_api = {
	.on = sf32lb_clock_control_on,
	.off = sf32lb_clock_control_off,
	.get_rate = sf32lb_clock_control_get_rate,
};

/* Clock initialization function */
static int sf32lb_clock_init(const struct device *dev)
{
	const struct sf32lb_clock_config *config = dev->config;
	struct sf32lb_clock_data *data = dev->data;

	if (IS_ENABLED(SF32LB_HXT48M_ENABLED)) {
		HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_SYS, RCC_SYSCLK_HXT48);
		HAL_RCC_HCPU_ClockSelect(RCC_CLK_MOD_HP_PERI, RCC_CLK_PERI_HXT48);
	}

	if (IS_ENABLED(SF32_SYS_CLOCK_ENABLED)) {
		HAL_RCC_HCPU_SetDiv(SF32_SCLK_HDIV, SF32_SCLK_PDIV1, SF32_SCLK_PDIV2);
	}

	if (IS_ENABLED(SF32LB_LXT32K_ENABLED)) {
	}
#if 0
	LOG_INF("SF32LB clock initialized");
	LOG_INF("System clock: %d Hz", data->sys_clk_freq);
	LOG_INF("AHB clock: %d Hz", data->ahb_clk_freq);
	LOG_INF("APB1 clock: %d Hz", data->apb1_clk_freq);
	LOG_INF("APB2 clock: %d Hz", data->apb2_clk_freq);
#endif
	return 0;
}

/* Device configuration */
#define SF32LB_CLOCK_INIT(n) \
	static const struct sf32lb_clock_config sf32lb_clock_config_##n = { \
	}; \
	static struct sf32lb_clock_data sf32lb_clock_data_##n; \
	DEVICE_DT_INST_DEFINE(n, \
			     sf32lb_clock_init, \
			     NULL, \
			     &sf32lb_clock_data_##n, \
			     &sf32lb_clock_config_##n, \
			     PRE_KERNEL_1, \
			     CONFIG_CLOCK_CONTROL_INIT_PRIORITY, \
			     &sf32lb_clock_api);

DT_INST_FOREACH_STATUS_OKAY(SF32LB_CLOCK_INIT)
