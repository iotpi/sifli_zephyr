/*
 * Copyright (c) 2024 SiFli, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_CLOCK_CONTROL_SF32LB_CLOCK_CONTROL_H_
#define ZEPHYR_DRIVERS_CLOCK_CONTROL_SF32LB_CLOCK_CONTROL_H_

#include <zephyr/drivers/clock_control.h>

/* Clock subsystem identifiers */
#define SF32LB_CLOCK_SUBSYS_HSE    0
#define SF32LB_CLOCK_SUBSYS_HSI    1
#define SF32LB_CLOCK_SUBSYS_PLL    2
#define SF32LB_CLOCK_SUBSYS_SYSCLK 3
#define SF32LB_CLOCK_SUBSYS_AHB    4
#define SF32LB_CLOCK_SUBSYS_APB1   5
#define SF32LB_CLOCK_SUBSYS_APB2   6

/* RCC register definitions */
#define RCC_BASE           0x40021000
#define RCC                ((RCC_TypeDef *) RCC_BASE)

typedef struct {
    volatile uint32_t CR;      /* Clock control register */
    volatile uint32_t CFGR;    /* Clock configuration register */
    volatile uint32_t CIR;     /* Clock interrupt register */
    volatile uint32_t APB2RSTR;/* APB2 peripheral reset register */
    volatile uint32_t APB1RSTR;/* APB1 peripheral reset register */
    volatile uint32_t AHBENR;  /* AHB peripheral clock enable register */
    volatile uint32_t APB2ENR; /* APB2 peripheral clock enable register */
    volatile uint32_t APB1ENR; /* APB1 peripheral clock enable register */
    volatile uint32_t BDCR;    /* Backup domain control register */
    volatile uint32_t CSR;     /* Control/status register */
} RCC_TypeDef;

/* RCC_CR register bits */
#define RCC_CR_HSION      (1 << 0)  /* Internal high-speed clock enable */
#define RCC_CR_HSIRDY     (1 << 1)  /* Internal high-speed clock ready flag */
#define RCC_CR_HSEON      (1 << 16) /* External high-speed clock enable */
#define RCC_CR_HSERDY     (1 << 17) /* External high-speed clock ready flag */
#define RCC_CR_PLLON      (1 << 24) /* PLL enable */
#define RCC_CR_PLLRDY     (1 << 25) /* PLL clock ready flag */

/* RCC_CFGR register bits */
#define RCC_CFGR_SW       (0x3 << 0)  /* System clock switch */
#define RCC_CFGR_SW_HSI   (0x0 << 0)  /* HSI selected as system clock */
#define RCC_CFGR_SW_HSE   (0x1 << 0)  /* HSE selected as system clock */
#define RCC_CFGR_SW_PLL   (0x2 << 0)  /* PLL selected as system clock */
#define RCC_CFGR_SWS      (0x3 << 2)  /* System clock switch status */
#define RCC_CFGR_SWS_HSI  (0x0 << 2)  /* HSI used as system clock */
#define RCC_CFGR_SWS_HSE  (0x1 << 2)  /* HSE used as system clock */
#define RCC_CFGR_SWS_PLL  (0x2 << 2)  /* PLL used as system clock */

/* PLL configuration */
#define RCC_CFGR_PLLSRC   (1 << 16)  /* PLL entry clock source */
#define RCC_CFGR_PLLSRC_HSI (0 << 16) /* HSI oscillator clock selected as PLL input clock */
#define RCC_CFGR_PLLSRC_HSE (1 << 16) /* HSE oscillator clock selected as PLL input clock */

#define RCC_CFGR_PLLMUL   (0xF << 18) /* PLL multiplication factor */
#define RCC_CFGR_PLLMUL_2  (0x0 << 18) /* PLL input clock * 2 */
#define RCC_CFGR_PLLMUL_3  (0x1 << 18) /* PLL input clock * 3 */
#define RCC_CFGR_PLLMUL_4  (0x2 << 18) /* PLL input clock * 4 */
#define RCC_CFGR_PLLMUL_5  (0x3 << 18) /* PLL input clock * 5 */
#define RCC_CFGR_PLLMUL_6  (0x4 << 18) /* PLL input clock * 6 */
#define RCC_CFGR_PLLMUL_7  (0x5 << 18) /* PLL input clock * 7 */
#define RCC_CFGR_PLLMUL_8  (0x6 << 18) /* PLL input clock * 8 */
#define RCC_CFGR_PLLMUL_9  (0x7 << 18) /* PLL input clock * 9 */
#define RCC_CFGR_PLLMUL_10 (0x8 << 18) /* PLL input clock * 10 */
#define RCC_CFGR_PLLMUL_11 (0x9 << 18) /* PLL input clock * 11 */
#define RCC_CFGR_PLLMUL_12 (0xA << 18) /* PLL input clock * 12 */
#define RCC_CFGR_PLLMUL_13 (0xB << 18) /* PLL input clock * 13 */
#define RCC_CFGR_PLLMUL_14 (0xC << 18) /* PLL input clock * 14 */
#define RCC_CFGR_PLLMUL_15 (0xD << 18) /* PLL input clock * 15 */
#define RCC_CFGR_PLLMUL_16 (0xE << 18) /* PLL input clock * 16 */

/* AHB, APB1, APB2 prescalers */
#define RCC_CFGR_HPRE     (0xF << 4)  /* AHB prescaler */
#define RCC_CFGR_HPRE_DIV1  (0x0 << 4) /* AHB clock not divided */
#define RCC_CFGR_HPRE_DIV2  (0x8 << 4) /* AHB clock divided by 2 */
#define RCC_CFGR_HPRE_DIV4  (0x9 << 4) /* AHB clock divided by 4 */
#define RCC_CFGR_HPRE_DIV8  (0xA << 4) /* AHB clock divided by 8 */
#define RCC_CFGR_HPRE_DIV16 (0xB << 4) /* AHB clock divided by 16 */

#define RCC_CFGR_PPRE1    (0x7 << 8)  /* APB1 prescaler */
#define RCC_CFGR_PPRE1_DIV1  (0x0 << 8) /* APB1 clock not divided */
#define RCC_CFGR_PPRE1_DIV2  (0x4 << 8) /* APB1 clock divided by 2 */
#define RCC_CFGR_PPRE1_DIV4  (0x5 << 8) /* APB1 clock divided by 4 */
#define RCC_CFGR_PPRE1_DIV8  (0x6 << 8) /* APB1 clock divided by 8 */
#define RCC_CFGR_PPRE1_DIV16 (0x7 << 8) /* APB1 clock divided by 16 */

#define RCC_CFGR_PPRE2    (0x7 << 11) /* APB2 prescaler */
#define RCC_CFGR_PPRE2_DIV1  (0x0 << 11) /* APB2 clock not divided */
#define RCC_CFGR_PPRE2_DIV2  (0x4 << 11) /* APB2 clock divided by 2 */
#define RCC_CFGR_PPRE2_DIV4  (0x5 << 11) /* APB2 clock divided by 4 */
#define RCC_CFGR_PPRE2_DIV8  (0x6 << 11) /* APB2 clock divided by 8 */
#define RCC_CFGR_PPRE2_DIV16 (0x7 << 11) /* APB2 clock divided by 16 */

/* Flash latency configuration */
#define FLASH_ACR_LATENCY (0xF << 0)  /* Flash latency */
#define FLASH_ACR_LATENCY_0 (0x0 << 0) /* Zero wait state */
#define FLASH_ACR_LATENCY_1 (0x1 << 0) /* One wait state */
#define FLASH_ACR_LATENCY_2 (0x2 << 0) /* Two wait states */


/* Clock frequency limits */
#define SF32LB_MAX_SYSCLK_FREQ 72000000  /* Maximum system clock frequency */
#define SF32LB_MAX_AHB_FREQ    72000000  /* Maximum AHB frequency */
#define SF32LB_MAX_APB1_FREQ   36000000  /* Maximum APB1 frequency */
#define SF32LB_MAX_APB2_FREQ   72000000  /* Maximum APB2 frequency */

#endif /* ZEPHYR_DRIVERS_CLOCK_CONTROL_SF32LB_CLOCK_CONTROL_H_ */ 