/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2016 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT st_bluenrg_uart

/**
 * @brief Driver for UART port on BlueNRG family processor.
 * @note  LPUART and U(S)ART have the same base and
 *        majority of operations are performed the same way.
 *        Please validate for newly added series.
 */
#include <zephyr/sys/printk.h>


#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/sys/__assert.h>
#include <soc.h>
#include <zephyr/init.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/reset.h>


#include <zephyr/linker/sections.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include "uart_bluenrg.h"

#include <rf_driver_ll_usart.h>
#include <rf_driver_ll_lpuart.h>

#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
LOG_MODULE_REGISTER(uart_bluenrg, CONFIG_UART_LOG_LEVEL);
