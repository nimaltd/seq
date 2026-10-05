/**
 * @file        main.h
 * @brief       Host stub standing in for the CubeMX generated main.h.
 * @version     2.0.0
 *
 * @author      Nima Askari (NimaLTD)
 * @email       nima.askari@gmail.com
 * @github      https://www.github.com/nimaltd
 *
 * @copyright   (c) 2026 Nima Askari (NimaLTD)
 *              SPDX-License-Identifier: Apache-2.0
 *              See LICENSE.md in the project root for the full license text.
 *
 * @note        This file exists only so the library can be compiled and tested
 *              on a PC. It is not part of the shipped library, and it is never
 *              on the include path of a real STM32 build.
 */

#ifndef MAIN_H
#define MAIN_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <stdint.h>

/*
 * ****************************************************************************************************
 * Macros
 * ****************************************************************************************************
*/

/* CMSIS spells volatile this way. */
#define __IO                volatile

/* As the HAL defines it with USE_FULL_ASSERT set, so the tests see a NULL
   argument stopped. The tests define assert_failed(). */
#define assert_param(expr)  ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))

/* The tests are single threaded, so ordering needs no barrier here. */
#define __DMB()

/* PRIMASK, modelled closely enough that a test can tell whether the library
   really closed the window an interrupt could arrive through. The fake
   interrupt in the tests checks seq_test_irq_enabled() before it fires, which
   is exactly what the hardware does. */
extern int seq_test_primask;

/* Restoring PRIMASK goes through a function, because re-enabling interrupts is
   the moment a pending one actually fires, and the tests have to model that. */
extern void seq_test_set_primask(uint32_t value);

#define __get_PRIMASK()   ((uint32_t)seq_test_primask)
#define __set_PRIMASK(x)  seq_test_set_primask((uint32_t)(x))
#define __disable_irq()   (seq_test_primask = 1)
#define __enable_irq()    seq_test_set_primask(0U)

/* Fires where a higher priority interrupt could land. Compiled out of a real
   build, so it costs nothing on the target. */
extern void seq_test_hook(void);
#define SEQ_TEST_HOOK() seq_test_hook()

/*
 * ****************************************************************************************************
 * Public function prototypes
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Where a failed assert_param lands. The tests define it.
 */
void assert_failed(uint8_t *file, uint32_t line);

/*****************************************************************************************************/
/**
 * @brief Return the current tick. Backed by a value the tests control.
 */
uint32_t HAL_GetTick(void);

#endif /* MAIN_H */
