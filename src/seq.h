/**
 * @file        seq.h
 * @brief       Non blocking state sequencer and interrupt task queue for STM32.
 * @version     2.1.0
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
 */

#ifndef SEQ_H
#define SEQ_H

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include <stdbool.h>
#include <stdint.h>

#include "seq_config.h"

/*
 * ****************************************************************************************************
 * Configuration checks
 * ****************************************************************************************************
*/

/* Checked here and never in seq_config.h. That file is the user's: it is copied
   once and never replaced, so a check in it can be edited away and would never
   reach anyone who installed before it was added. */

/* One slot is always kept free so a full queue can be told apart from an empty
   one, which leaves SEQ_MAX_TASKS - 1 usable. With 1 there would be none, and
   every seq_task_add() would quietly return SEQ_ERR_FULL. */
#ifndef SEQ_MAX_TASKS
#error "SEQ_MAX_TASKS is not defined. Add it to seq_config.h"
#elif SEQ_MAX_TASKS < 2U
#error "SEQ_MAX_TASKS must be at least 2"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * ****************************************************************************************************
 * Types
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief State machine handle, declared ahead so a state function can take one.
 */
typedef struct seq_s seq_t;

/*****************************************************************************************************/
/**
 * @brief A state function. Gets its own handle and the argument it was entered with.
 */
typedef void (*seq_state_fn_t)(seq_t *handle, void *arg);

/*****************************************************************************************************/
/**
 * @brief A queued task. Gets whatever was handed to seq_task_add().
 */
typedef void (*seq_task_fn_t)(void *arg);

/*****************************************************************************************************/
/**
 * @brief Error values returned by the task queue.
 */
typedef enum
{
    SEQ_ERR_NONE    = 0, /**< The task was queued.        */
    SEQ_ERR_FULL    = 1, /**< The task queue is full.     */
    SEQ_ERR_INVALID = 2, /**< Not returned since 2.1.0.   */

} seq_err_t;

/*****************************************************************************************************/
/**
 * @brief State machine handle. Declare one per state machine.
 */
struct seq_s
{
    seq_state_fn_t next_fn;   /**< State function to run next.                    */
    void           *arg;      /**< Handed to next_fn every time it runs.          */
    uint32_t       time;      /**< Tick value when the current state was entered. */
    uint32_t       wait_ms;   /**< How long to wait before the next state runs.   */
    uint8_t        entering;  /**< Set until the next state has run once.         */
    uint8_t        first_run; /**< Set during the first run of the current state. */
};

/*
 * ****************************************************************************************************
 * Public function prototypes
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Initialize a handle and set the state it starts from, with its argument.
 */
void seq_init(seq_t *handle, seq_state_fn_t first_fn, void *arg);

/*****************************************************************************************************/
/**
 * @brief Run the queued tasks and the current state. Call this from the main loop.
 */
void seq_loop(seq_t *handle);

/*****************************************************************************************************/
/**
 * @brief Choose the next state and its argument, and how many milliseconds to wait first.
 */
void seq_next(seq_t *handle, seq_state_fn_t next_fn, void *arg, uint32_t wait_ms);

/*****************************************************************************************************/
/**
 * @brief Get how long the machine has been in the current state, in milliseconds.
 */
uint32_t seq_time(const seq_t *handle);

/*****************************************************************************************************/
/**
 * @brief Whether the running state is on its first run since it was entered.
 */
bool seq_first_run(const seq_t *handle);

/*****************************************************************************************************/
/**
 * @brief Stop the machine. No state runs until seq_next() or seq_init() is called.
 */
void seq_stop(seq_t *handle);

/*****************************************************************************************************/
/**
 * @brief Whether the machine has a state to run. False after seq_stop().
 */
bool seq_running(const seq_t *handle);

/*****************************************************************************************************/
/**
 * @brief The most tasks that have ever been queued at once, for sizing SEQ_MAX_TASKS.
 */
uint32_t seq_task_peak(void);

/*****************************************************************************************************/
/**
 * @brief Drop every queued task. Call it from the main loop or a task, not an interrupt.
 */
void seq_task_flush(void);

/*****************************************************************************************************/
/**
 * @brief Queue a task with its argument. Safe to call from an interrupt.
 */
seq_err_t seq_task_add(seq_task_fn_t task_fn, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* SEQ_H */
