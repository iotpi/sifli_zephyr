# Copyright (c) 2024 SiFli
# SPDX-License-Identifier: Apache-2.0

# zephyr_library()

if(CONFIG_ARM)
  set(SOC_LINKER_SCRIPT ${ZEPHYR_BASE}/include/zephyr/arch/arm/cortex_m/scripts/linker.ld CACHE INTERNAL "SoC Linker script")
endif()

zephyr_compile_definitions(
  -DSOC_BF0_HCPU
  -DHAL_TICK_PER_SECOND=1000
  -D__HAL_ROM_USED=100
  -DUSE_HAL_DRIVER
)

add_subdirectory(${SOC_SERIES})
