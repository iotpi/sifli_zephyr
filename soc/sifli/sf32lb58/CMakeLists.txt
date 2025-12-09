zephyr_include_directories(.)

if (CONFIG_SOC_SF32LB58_HCPU)
zephyr_sources(soc_hcpu.c)
zephyr_compile_definitions(
  -DSOC_BF0_HCPU
  # -DHAL_TICK_PER_SECOND=1000
  # -D__HAL_ROM_USED=100
  # -DUSE_HAL_DRIVER
)

endif()
