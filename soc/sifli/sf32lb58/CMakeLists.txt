zephyr_include_directories(.)

if (CONFIG_SOC_SF32LB58_HCPU)
zephyr_sources(soc_hcpu.c)
endif()

zephyr_compile_definitions()
