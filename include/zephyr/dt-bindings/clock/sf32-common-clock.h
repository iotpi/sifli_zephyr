/*
 * Copyright (c) 2025, Qingsong Gou <gouqs@hotmail.com>
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_SF32_COMMON_CLOCK_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_SF32_COMMON_CLOCK_H_

#define SF32_CLOCK_REG_OFFSET 5

#define SF32_RCM_REG_OFFSET(clk_cfg) (clk_cfg >> SF32_CLOCK_REG_OFFSET)
#define SF32_RCM_BIT_OFFSET(clk_cfg) (clk_cfg & 0x1f)

#define SF32_RCM_FIELD_DEF_OFFSET(address, offset) (((address) << SF32_CLOCK_REG_OFFSET) | (offset))

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_SF32_COMMON_CLOCK_H_ */
