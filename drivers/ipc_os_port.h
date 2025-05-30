/**
  ******************************************************************************
  * @file   ipc_os_port.h
  * @author Sifli software development team
  * @brief Sifli ipc hw interface
  * @{
  ******************************************************************************
*/
/*
 * @attention
 * Copyright (c) 2019 - 2022,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated circuit
 *    in a product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef IPC_OS_PORT_H
#define IPC_OS_PORT_H
#include <stdint.h>
#include <zephyr/kernel.h>


#ifdef __cplusplus
extern "C" {
#endif

#define os_interrupt_disable()      irq_lock()

#define os_interrupt_enable(mask)   irq_unlock(mask)

#define os_interrupt_enter()
#define os_interrupt_exit()


/// @}  file

#ifdef __cplusplus
}
#endif

# Add HAL source code
file(GLOB SF32LB_SOURCES "../../SifliSDK/drivers/hal/*.c")
foreach(FILE ${SF32LB_SOURCES})
    string(FIND "${FILE}" "bf0_hal_audcodec_m.c" POSITION)
    if(POSITION GREATER -1 AND CONFIG_SOC_SERIES_SF32LB56X)
        message("Remove ${FILE} from compiling.")
    else()
        string(FIND "${FILE}" "bf0_hal_audcodec.c" POSITION)
        if(POSITION GREATER -1 AND CONFIG_SOC_SERIES_SF32LB52X)
            message("Remove ${FILE} from compiling.")
        else()
            zephyr_library_sources(${FILE})
        endif()
    endif()
endforeach()

# Relocate HAL source code to RAM
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal_mpi.c LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal_mpi_ex.c LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal_mpi_psram.c LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal_mpi_psram.c LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal_rcc.c LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal_hpaon.c LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal_pinmux.c LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal_gpio.c LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/flash_table.c LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal.c FILTER ".*\\.HAL_Init|.*\\.HAL_PostMspInit|.*\\.HAL_Delay|.*\\.HAL_Delay_us|.*\\.HAL_Delay_us_" LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_hal_pmu.c FILTER ".*\\.HAL_PMU_SET_HXT_CBANK|.*\\.HAL_PMU_GetHpsysVoutRef|.*\\.HAL_PMU_GetHpsysVoutRef2" LOCATION RAM NOKEEP)
zephyr_code_relocate(FILES ../../SifliSDK/drivers/hal/bf0_sys_cfg.c FILTER ".*\\BSP_HxtCbank_Config" LOCATION RAM NOKEEP)


# Add 56x specific code
if (CONFIG_SOC_SERIES_SF32LB56X)
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb56x/Templates/system_bf0_ap.c")
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb56x/bt_rf_fulcal.c")
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb56x/bf0_pin_const.c")
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb56x/bf0_lcpu_init.c")
    zephyr_code_relocate(FILES ../../SifliSDK/drivers/cmsis/sf32lb56x/bf0_pin_const.c LOCATION RAM NOKEEP)
endif()

# Add 52x specific code
if (CONFIG_SOC_SERIES_SF32LB52X)
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb52x/Templates/system_bf0_ap.c")
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb52x/bt_rf_fulcal.c")
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb52x/bf0_pin_const.c")
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb52x/bf0_lcpu_init.c")
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb52x/lcpu_patch.c")
	zephyr_library_sources("../../SifliSDK/drivers/cmsis/sf32lb52x/lcpu_patch_rev_b.c")
    zephyr_code_relocate(FILES ../../SifliSDK/drivers/cmsis/sf32lb52x/bf0_pin_const.c LOCATION RAM NOKEEP)
endif()

# Add Bluetooth HCI
if (CONFIG_BT)
	file(GLOB SF32LB_IPC "../../SifliSDK/middleware/ipc_queue/common/*.c")
	foreach(FILE ${SF32LB_IPC})
        zephyr_library_sources(${FILE})
	endforeach()
	zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../SifliSDK/middleware/include)
	zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../SifliSDK/middleware/ipc_queue/common)
	if (CONFIG_SOC_SERIES_SF32LB52X)
		file(GLOB SF32LB_IPC_PORT "../../SifliSDK/middleware/ipc_queue/porting/sf32lb52x/hcpu/*.c")
		zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../SifliSDK/middleware/ipc_queue/porting/sf32lb52x/hcpu)
	endif()
	if (CONFIG_SOC_SERIES_SF32LB56X)
		file(GLOB SF32LB_IPC_PORT "../../SifliSDK/middleware/ipc_queue/porting/sf32lb56x/hcpu/*.c")
		zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../SifliSDK/middleware/ipc_queue/porting/sf32lb56x/hcpu)
	endif()
	foreach(FILE ${SF32LB_IPC_PORT})
        zephyr_library_sources(${FILE})
	endforeach()
endif()

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/.)
zephyr_library_sources_ifdef(CONFIG_GPIO sflb32_gpio.c)
zephyr_library_sources_ifdef(CONFIG_CONSOLE sflb32_console.c)
zephyr_library_sources_ifdef(CONFIG_SERIAL sflb32_serial.c)
zephyr_library_sources_ifdef(CONFIG_BT sflb32_bthci.c)
zephyr_library_sources_ifdef(CONFIG_FLASH sflb32_flash.c)


/// @}
#endif
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/

