/*
 * Copyright (c) 2025, Qingsong Gou <gouqs@foxmail.com>
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_SF32LB525_CLOCK_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_SF32LB525_CLOCK_H_

#define SF32LB_CLOCK(offset, pos) ((offset) << 6 | (pos))

/**
 * clock offset
 */
#define ECR1 0
#define ECR2 1

/**
 * clock id
 */
#define DMAC1       (0)
#define MAILBOX1    (1)
#define PINMUX1     (2)
#define USART2      (4)
#define EZIP1       (5)
#define EPIC        (6)
#define LCDC1       (7)
#define I2S1        (8)
#define SYSCFG1     (10)
#define EFUSEC      (11)
#define AES         (12)
#define CRC1        (13)
#define TRNG        (14)
#define GPTIM1      (15)
#define GPTIM2      (16)
#define BTIM1       (17)
#define BTIM2       (18)
#define SPI1        (20)
#define SPI2        (21)
#define EXTDMA      (22)
#define SECU1       (23)
#define PDM1        (25)
#define I2C1        (27)
#define I2C2        (28)
#define PTC1        (31)

#define GPIO1       (0)
#define MPI1        (1)
#define MPI2        (2)
#define SDMMC1      (4)
#define USBC        (6)
#define I2C3        (8)
#define ATIM1       (9)
#define USART3      (12)
#define AUDCODEC    (19)
#define AUDPRC      (20)
#define GPADC       (22)
#define TSEN        (23)
#define I2C4        (24)

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_SF32LB525_CLOCK_H_ */