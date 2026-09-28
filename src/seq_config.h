/**
 * @file        seq_config.h
 * @brief       Build time configuration for the sequencer library.
 * @version     2.0.1
 *
 * @author      Nima Askari (NimaLTD)
 * @email       nima.askari@gmail.com
 * @github      https://www.github.com/nimaltd
 * @linkedin    https://www.linkedin.com/in/nimaltd
 * @youtube     https://www.youtube.com/@nimaltd
 * @instagram   https://instagram.com/github.nimaltd
 *
 * @copyright   (c) 2026 Nima Askari (NimaLTD)
 *              SPDX-License-Identifier: Apache-2.0
 *              See LICENSE.md in the project root for the full license text.
 *
 * @note        Your copy of this file is yours. The installer creates it once
 *              and never touches it again, so updating the library cannot
 *              overwrite a setting you changed.
 */

#ifndef SEQ_CONFIG_H
#define SEQ_CONFIG_H

/*
 * ****************************************************************************************************
 * Configuration
 * ****************************************************************************************************
*/

/* USER CODE BEGIN SEQ_CONFIGURATION */

/* Slots in the task queue. One slot is always kept free so a full queue can be
   told apart from an empty one, so the queue holds SEQ_MAX_TASKS - 1 tasks.
   Must be at least 2. */
#define SEQ_MAX_TASKS       16U

/* USER CODE END SEQ_CONFIGURATION */

#endif /* SEQ_CONFIG_H */
