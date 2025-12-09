# Copyright (c) 2025 Qingdao IotPi Ltd.
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_PINCTRL_SF32LB58 pinctrl_sf32lb58_pinmux.c pinctrl_sf32lb58_pinctrl.c)
