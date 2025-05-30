/*
 * Copyright (c) 2025, Qingsong Gou <gouqs@hotmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT sf32lb_wdt

#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <zephyr/sys_clock.h>
#include <zephyr/drivers/clock_control.h>

LOG_MODULE_REGISTER(wdt_sf32lb, CONFIG_WDT_LOG_LEVEL);

struct wdt_sf32lb_config {
    uint32_t base;
    const struct device *clock_dev;
    const clock_control_subsys_t clock_subsys;
};

struct wdt_sf32lb_data {
    uint32_t timeout;
    bool window_enabled;
    bool reset_enabled;
};

static int wdt_sf32lb_setup(const struct device *dev, uint8_t options)
{
    const struct wdt_sf32lb_config *config = dev->config;
    struct wdt_sf32lb_data *data = dev->data;
    uint32_t reg_val;

    /* Configure watchdog */
    reg_val = WDT_CR_EN;
    
    if (options & WDT_OPT_RESET) {
        reg_val |= WDT_CR_RSTEN;
    }
    
    if (options & WDT_OPT_WINDOW) {
        reg_val |= WDT_CR_WINEN;
    }

    /* Write key to enable configuration */
    sys_write32(WDT_CR_KEY, config->base + WDT_CR_OFFSET);
    sys_write32(reg_val, config->base + WDT_CR_OFFSET);

    /* Set timeout value */
    sys_write32(data->timeout, config->base + WDT_RLR_OFFSET);

    return 0;
}

static int wdt_sf32lb_disable(const struct device *dev)
{
    const struct wdt_sf32lb_config *config = dev->config;

    /* Write key to disable watchdog */
    sys_write32(WDT_CR_KEY, config->base + WDT_CR_OFFSET);
    sys_write32(0, config->base + WDT_CR_OFFSET);

    return 0;
}

static int wdt_sf32lb_install_timeout(const struct device *dev,
                                    const struct wdt_timeout_cfg *cfg)
{
    struct wdt_sf32lb_data *data = dev->data;

    if (cfg->window.min != 0) {
        return -ENOTSUP;
    }

    if (cfg->window.max > WDT_RLR_MAX) {
        return -EINVAL;
    }

    data->timeout = cfg->window.max;
    data->window_enabled = (cfg->window.min != 0);
    data->reset_enabled = (cfg->flags & WDT_FLAG_RESET_SOC) != 0;

    return 0;
}

static int wdt_sf32lb_feed(const struct device *dev, int channel_id)
{
    const struct wdt_sf32lb_config *config = dev->config;

    /* Write key to reload watchdog */
    sys_write32(WDT_CR_KEY, config->base + WDT_CR_OFFSET);
    sys_write32(WDT_CR_EN, config->base + WDT_CR_OFFSET);

    return 0;
}

static const struct wdt_driver_api wdt_sf32lb_api = {
    .setup = wdt_sf32lb_setup,
    .disable = wdt_sf32lb_disable,
    .install_timeout = wdt_sf32lb_install_timeout,
    .feed = wdt_sf32lb_feed,
};

static int wdt_sf32lb_init(const struct device *dev)
{
    const struct wdt_sf32lb_config *config = dev->config;
    struct wdt_sf32lb_data *data = dev->data;
    uint32_t reg_val;

    if (!device_is_ready(config->clock_dev)) {
        LOG_ERR("Clock control device not ready");
        return -ENODEV;
    }

    /* Enable WDT clock */
    if (clock_control_on(config->clock_dev, config->clock_subsys) != 0) {
        LOG_ERR("Failed to enable WDT clock");
        return -EIO;
    }

    /* Initialize data structure */
    data->timeout = WDT_RLR_MAX;  /* Set default timeout to maximum */
    data->window_enabled = false;
    data->reset_enabled = true;   /* Enable reset by default */

    /* Configure watchdog with default settings */
    reg_val = WDT_CR_EN | WDT_CR_RSTEN;  /* Enable WDT and reset */

    /* Write key to enable configuration */
    sys_write32(WDT_CR_KEY, config->base + WDT_CR_OFFSET);
    sys_write32(reg_val, config->base + WDT_CR_OFFSET);

    /* Set default timeout value */
    sys_write32(data->timeout, config->base + WDT_RLR_OFFSET);

    LOG_INF("Watchdog initialized with timeout %u", data->timeout);

    return 0;
}

#define WDT_SF32LB_INIT(n) \
    static struct wdt_sf32lb_data wdt_sf32lb_data_##n; \
    static const struct wdt_sf32lb_config wdt_sf32lb_config_##n = { \
        .base = DT_INST_REG_ADDR(n), \
        .clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)), \
        .clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, id), \
    }; \
    DEVICE_DT_INST_DEFINE(n, \
                         wdt_sf32lb_init, \
                         NULL, \
                         &wdt_sf32lb_data_##n, \
                         &wdt_sf32lb_config_##n, \
                         POST_KERNEL, \
                         CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
                         &wdt_sf32lb_api);

DT_INST_FOREACH_STATUS_OKAY(WDT_SF32LB_INIT)
