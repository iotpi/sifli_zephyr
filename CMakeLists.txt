# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories_ifdef(CONFIG_SOC_FAMILY_SF32LB include)

add_subdirectory(modules)
add_subdirectory(drivers)

add_subdirectory_ifdef(CONFIG_SOC_FAMILY_SF32LB soc/sifli)
