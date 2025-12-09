/*
 * Copyright (c) 2025 Qingdao IotPi Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_ARM_SIFLI_SF32LB58_PINCTRL_SF32LB58_H_
#define ZEPHYR_SOC_ARM_SIFLI_SF32LB58_PINCTRL_SF32LB58_H_

#include <zephyr/devicetree.h>
#include <zephyr/types.h>

struct pinctrl_soc_pinmux {
  uint32_t reg;
  uint8_t sel;
};

struct pinctrl_soc_pin {
  struct pinctrl_soc_pinmux pinmux;
  uint32_t flags;
};

typedef struct pinctrl_soc_pin pinctrl_soc_pin_t;

#define SF32LB58_PINMUX(node_id)                    \
  {                                                 \
    .reg = DT_PROP_BY_IDX(node_id, pinmux, 0),      \
    .sel = DT_PROP_BY_IDX(node_id, pinmux, 1),      \
  }

#define PINCTRL_PINMUX(group_id, prop, idx)         \
  SF32LB58_PINMUX(DT_PHANDLE_BY_IDX(group_id, prop, idx)

#define PINCTRL_STATE_PIN_INIT(group_id, prop, idx) \
  {                                                 \
    .pinmux = PINCTRL_PINMUX(group_id, prop, idx),  \
    .flags = 0,                                     \
  },


#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop)      \
  {                                                   \
    DT_FOREACH_CHILD_VARGS(DT_PHANDLE(node_id, prop), \
                           DT_FOREACH_PROP_ELEM,      \
                           PINCTRL_STATE_PIN_INIT)    \
  };

#endif /* ZEPHYR_SOC_ARM_SIFLI_SF32LB58_PINCTRL_SF32LB58_H_ */
