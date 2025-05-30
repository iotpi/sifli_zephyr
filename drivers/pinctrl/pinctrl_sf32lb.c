#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#include "bf0_hal_pinmux.h"

#define DT_DRV_COMPAT sifli_sf32lb_pinctrl

LOG_MODULE_REGISTER(pinctrl_sflb32, CONFIG_PINCTRL_LOG_LEVEL);

static int pinctrl_configure_pin(uint32_t pad, uint32_t func)
{
	HAL_PIN_Select(pad, func, 1);

	return 0;
}

int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg)
{
	ARG_UNUSED(reg);

	for (uint8_t i = 0U; i < pin_cnt; i++) {
		uint32_t pad = SFLB32_GET_PAD(pins[i]);
		uint32_t func = SFLB32_GET_FUNC(pins[i]);

		if (pinctrl_configure_pin(pad, func) != 0) {
			LOG_ERR("Failed to configure pin %u", pad);
			return -EIO;
		}
	}
	return 0;
}