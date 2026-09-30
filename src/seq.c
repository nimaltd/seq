/**
 * @file        seq.c
 * @brief       Non blocking state sequencer and interrupt task queue for STM32.
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
 */

/*
 * ****************************************************************************************************
 * Includes
 * ****************************************************************************************************
*/

#include "seq.h"
#include <stddef.h>
#include "main.h"

/* A place for the tests to simulate a higher priority interrupt arriving at the
   worst possible moment. Nothing is generated unless the tests define it. */
#ifndef SEQ_TEST_HOOK
#define SEQ_TEST_HOOK() do { } while (0)
#endif

/*
 * ****************************************************************************************************
 * Types
 * ****************************************************************************************************
*/

/* A plain alias, so __IO lands on the pointer rather than on what it points at.
   Spelling the member "__IO void *arg[]" would give an array of pointers to
   volatile void, which says the opposite of what the queue needs. */
typedef void *seq_arg_t;

/*****************************************************************************************************/
/**
 * @brief Task queue shared between interrupt context and the main loop.
 */
typedef struct
{
    __IO seq_task_fn_t fn[SEQ_MAX_TASKS];  /**< Circular buffer of queued tasks. */
    __IO seq_arg_t     arg[SEQ_MAX_TASKS]; /**< The argument each one gets.      */
    __IO uint32_t      head;               /**< Slot the producer writes next.   */
    __IO uint32_t      tail;               /**< Slot the consumer reads next.    */
    __IO uint32_t      peak;               /**< Deepest the queue has ever been. */

} seq_queue_t;

/*
 * ****************************************************************************************************
 * Global variables
 * ****************************************************************************************************
*/

/* Static storage is zero initialized by the language, which is exactly the
   empty queue, so spelling out an initializer here would add nothing. */
static seq_queue_t seq_queue;

/*
 * ****************************************************************************************************
 * Private function prototypes
 * ****************************************************************************************************
*/

static void seq_enter(seq_t *handle);
static void seq_queue_run(void);

/*
 * ****************************************************************************************************
 * Public function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Initialize a handle and set the state it starts from, with its argument.
 *
 * The machine runs first_fn on the next call to seq_loop(), with no wait, and
 * hands it arg exactly the way seq_next() hands the next state its argument.
 * This is simply the first transition.
 *
 * Every state is handed its own handle, so one set of state functions can drive
 * several machines. For data that belongs to a machine for its whole life, make
 * seq_t the first member of your own struct and cast the handle back to that
 * struct inside the state. The C standard guarantees a pointer to a struct and
 * a pointer to its first member are interchangeable.
 *
 * @param[out] handle    Handle to initialize. Must not be NULL.
 * @param[in]  first_fn  State function to start from. Must not be NULL.
 * @param[in]  arg       Handed to first_fn every time it runs. May be NULL.
 */
void seq_init(seq_t *handle, seq_state_fn_t first_fn, void *arg)
{
    assert_param(handle != NULL);
    assert_param(first_fn != NULL);

    if ((handle != NULL) && (first_fn != NULL))
    {
        handle->next_fn   = first_fn;
        handle->arg       = arg;
        handle->wait_ms   = 0U;
        handle->time      = HAL_GetTick(); /* Sane value for seq_time() before the first run. */
        handle->entering  = 1U;
        handle->first_run = 0U;
    }
}

/*****************************************************************************************************/
/**
 * @brief Run the queued tasks and the current state.
 *
 * Call this as often as possible from the main loop. Queued tasks are served
 * before the state function, so work handed over by an interrupt is not held
 * up by a state that is still waiting to run.
 *
 * @param[in,out] handle  Handle to run. Must not be NULL.
 */
void seq_loop(seq_t *handle)
{
    assert_param(handle != NULL);

    if (handle != NULL)
    {
        seq_queue_run();

        if (handle->next_fn != NULL)
        {
            if (handle->wait_ms == 0U)
            {
                seq_enter(handle);
                handle->next_fn(handle, handle->arg);
            }
            else if ((HAL_GetTick() - handle->time) >= handle->wait_ms)
            {
                /* Clear the wait before the state runs, so the state itself is
                   free to ask for a new one. */
                handle->wait_ms = 0U;
                seq_enter(handle);
                handle->next_fn(handle, handle->arg);
            }
            else
            {
                /* Still waiting, nothing to do. */
            }
        }
    }
}

/*****************************************************************************************************/
/**
 * @brief Choose the next state and its argument, and how long to wait before it runs.
 *
 * The wait is measured from this call, not from when the current state
 * returns. A wait of zero runs the next state on the following seq_loop().
 * Nothing blocks while it counts down: seq_loop() keeps returning at once, and
 * the task queue keeps being served.
 *
 * arg is handed to next_fn on every run, until the next transition replaces it.
 * Only the pointer is kept, not what it points at, so that has to stay valid
 * as long: a static, a global or a field of your own struct is fine. The
 * address of a local variable is not, since the state that called this has
 * returned long before the next one runs.
 *
 * @param[in,out] handle   Handle to update. Must not be NULL.
 * @param[in]     next_fn  State function to run next. Must not be NULL.
 * @param[in]     arg      Handed to next_fn every time it runs. May be NULL.
 * @param[in]     wait_ms  Milliseconds to wait before running next_fn.
 */
void seq_next(seq_t *handle, seq_state_fn_t next_fn, void *arg, uint32_t wait_ms)
{
    assert_param(handle != NULL);
    assert_param(next_fn != NULL);

    if ((handle != NULL) && (next_fn != NULL))
    {
        handle->wait_ms  = wait_ms;
        handle->time     = HAL_GetTick();
        handle->next_fn  = next_fn;
        handle->arg      = arg;
        handle->entering = 1U;
    }
}

/*****************************************************************************************************/
/**
 * @brief Get how long the machine has been in the current state.
 *
 * The clock restarts when a transition is requested with seq_init() or
 * seq_next(), and again when the next state actually starts running, so a
 * state can use it to measure its own runtime. While a transition is still
 * waiting to run, it measures the wait so far.
 *
 * @param[in] handle  Handle to read. Must not be NULL.
 * @return Milliseconds since the last transition, or 0 if handle is NULL or
 *         the machine is stopped.
 */
uint32_t seq_time(const seq_t *handle)
{
    uint32_t elapsed = 0U;

    assert_param(handle != NULL);

    if ((handle != NULL) && (handle->next_fn != NULL))
    {
        elapsed = HAL_GetTick() - handle->time;
    }

    return elapsed;
}

/*****************************************************************************************************/
/**
 * @brief Whether the running state is on its first run since it was entered.
 *
 * For work a state does once when it starts, such as sending a request, before
 * it polls for the answer on the runs after. A state runs on every seq_loop()
 * until it moves on, so without this the request would go out on every pass.
 * seq_time() reading 0 is no substitute: the main loop can run many times in
 * one millisecond, and it reads 0 on all of them.
 *
 * Every transition counts as entering, one back into the same state included,
 * and after a wait the first run is the one the wait ends with. Asking for a
 * transition during the first run does not change the answer for the rest of
 * that run. Meant to be called from inside a state function.
 *
 * @param[in] handle  Handle to read. Must not be NULL.
 * @return true during the first run of the current state, false on the runs
 *         after it, once stopped, or if handle is NULL.
 */
bool seq_first_run(const seq_t *handle)
{
    assert_param(handle != NULL);

    return (handle != NULL) && (handle->first_run != 0U);
}

/*****************************************************************************************************/
/**
 * @brief Stop the machine. No state runs until seq_next() or seq_init() is called.
 *
 * Useful for a terminal state, which would otherwise have to keep scheduling
 * itself just to stay put. Queued tasks still run, because the queue belongs to
 * the application rather than to any one machine.
 *
 * @param[in,out] handle  Handle to stop. Must not be NULL.
 */
void seq_stop(seq_t *handle)
{
    assert_param(handle != NULL);

    if (handle != NULL)
    {
        handle->next_fn   = NULL;
        handle->wait_ms   = 0U;
        handle->entering  = 0U;
        handle->first_run = 0U;
    }
}

/*****************************************************************************************************/
/**
 * @brief Whether the machine has a state to run.
 *
 * @param[in] handle  Handle to read. Must not be NULL.
 * @return true while a state is scheduled, false after seq_stop().
 */
bool seq_running(const seq_t *handle)
{
    assert_param(handle != NULL);

    return (handle != NULL) && (handle->next_fn != NULL);
}

/*****************************************************************************************************/
/**
 * @brief The most tasks that have ever been queued at once.
 *
 * There to size SEQ_MAX_TASKS by measurement rather than by guessing. A full
 * queue is reported by seq_task_add(), but that call is usually made from an
 * interrupt where nobody checks the result, so this is in practice the only way
 * to find out the queue ever came close to overflowing.
 *
 * @return Peak number of queued tasks since reset.
 */
uint32_t seq_task_peak(void)
{
    /* An aligned 32-bit load is atomic on Cortex-M, so no critical section is
       needed to read the peak. */
    return seq_queue.peak;
}

/*****************************************************************************************************/
/**
 * @brief Drop every queued task.
 *
 * Call it from the main loop or from inside a task, not from an interrupt. It
 * moves the consumer's end of the queue, which only the main loop is allowed to
 * touch. Called from a task, it also drops the tasks still due in that pass.
 */
void seq_task_flush(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    seq_queue.tail = seq_queue.head;

    __set_PRIMASK(primask);
}

/*****************************************************************************************************/
/**
 * @brief Queue a task with its argument.
 *
 * Safe to call from an interrupt. The task runs later, from seq_loop(), which
 * is what keeps the interrupt handler short. The argument is copied into the
 * queue, but whatever it points at is not, so it has to outlive the call: a
 * peripheral handle or a static buffer is fine, a local variable is not.
 *
 * @param[in] task_fn  Task to queue. Must not be NULL.
 * @param[in] arg      Handed to the task when it runs. May be NULL.
 * @return SEQ_ERR_NONE if the task was queued, SEQ_ERR_FULL if the queue is
 *         full, or SEQ_ERR_INVALID if task_fn is NULL.
 */
seq_err_t seq_task_add(seq_task_fn_t task_fn, void *arg)
{
    seq_err_t err = SEQ_ERR_INVALID;

    assert_param(task_fn != NULL);

    if (task_fn != NULL)
    {
        err = SEQ_ERR_FULL;

        /* Claiming a slot is read, write, publish, and a higher priority
           interrupt landing in the middle of that would claim the same slot and
           one of the two tasks would vanish with no error reported. Saving and
           restoring PRIMASK, rather than simply enabling interrupts at the end,
           keeps this safe to call from code that already has them disabled. */
        uint32_t primask = __get_PRIMASK();
        __disable_irq();

        {
            uint32_t head      = seq_queue.head;
            uint32_t next_head = (head + 1U) % SEQ_MAX_TASKS;

            SEQ_TEST_HOOK();

            /* Leaving one slot free is what lets a full queue be told apart
               from an empty one, since both would otherwise have head == tail. */
            if (next_head != seq_queue.tail)
            {
                seq_queue.fn[head]  = task_fn;
                seq_queue.arg[head] = arg;

                /* The slot has to be visible before head publishes it, or the
                   main loop can read a stale pointer out of it. Both members
                   are written first, so a consumer that sees the new head sees
                   the task and its argument together. */
                __DMB();

                seq_queue.head = next_head;
                err            = SEQ_ERR_NONE;

                {
                    uint32_t depth = (next_head + SEQ_MAX_TASKS - seq_queue.tail) % SEQ_MAX_TASKS;

                    if (depth > seq_queue.peak)
                    {
                        seq_queue.peak = depth;
                    }
                }
            }
        }

        __set_PRIMASK(primask);
    }

    return err;
}

/*
 * ****************************************************************************************************
 * Private function implementations
 * ****************************************************************************************************
*/

/*****************************************************************************************************/
/**
 * @brief Mark the first run of a new state, and restart its clock.
 *
 * Called just before every run. first_run is set for the first run after a
 * transition and cleared on the next, which is what seq_first_run() reports.
 * The clock must not restart on every pass, or seq_time() would sit at zero for
 * a state that runs on every loop, and every timeout built on it would be dead.
 *
 * @param[in,out] handle  Handle being run.
 */
static void seq_enter(seq_t *handle)
{
    handle->first_run = handle->entering;

    if (handle->entering != 0U)
    {
        handle->entering = 0U;
        handle->time     = HAL_GetTick();
    }
}

/*****************************************************************************************************/
/**
 * @brief Run the tasks that were queued when this call began.
 *
 * A burst is cleared in one pass, so the last task of a burst does not wait a
 * loop iteration per task ahead of it. Work queued while those run waits for
 * the next pass, which is what stops the state machine from being starved.
 */
static void seq_queue_run(void)
{
    /* No critical section here, and that is deliberate. Each index has exactly
       one writer: producers write head, and only this consumer writes tail.
       Producers write fn[head] while this reads fn[tail], and the always free
       slot keeps head from ever reaching tail, so they never meet. A producer
       that interrupts this and reads a tail that has not moved yet simply sees
       the queue as one fuller than it is, which can refuse a task but cannot
       corrupt one. Disabling interrupts here would only lengthen interrupt
       latency. It holds only while seq_loop() has a single caller. */
    /* How many tasks were queued when this call began. Running that many and
       no more is what bounds the loop: anything queued while they run,
       including by a task queueing another, waits for the next pass. Draining
       to empty would never return if a task kept requeueing itself, or if an
       interrupt produced faster than this drains, and the state machine would
       never run again. */
    uint32_t count = (seq_queue.head + SEQ_MAX_TASKS - seq_queue.tail) % SEQ_MAX_TASKS;

    /* Stopping at an empty queue as well is what makes seq_task_flush() safe
       inside a task. It moves tail up to wherever head has got to, which is
       past the end of this pass if anything was queued meanwhile. The loop
       used to wait for tail to reach that end, so it went round the whole ring
       instead, running every stale task left in the slots, and once it came
       back to the flushing task's own slot it never returned. */
    while ((count > 0U) && (seq_queue.tail != seq_queue.head))
    {
        uint32_t      tail      = seq_queue.tail;
        seq_task_fn_t task_fn   = seq_queue.fn[tail];
        void          *task_arg = seq_queue.arg[tail];

        count--;

        /* Release the slot before running the task, so the task is free to
           queue another one without hitting a queue that is falsely full.
           Both members are read first. The free slot rule means a producer
           cannot reach this slot again before the next pass of this loop, so
           reading the argument afterwards would in fact still be correct, but
           it would be correct only because of an invariant kept elsewhere in
           this file. Reading both up front is correct on its own. */
        seq_queue.tail = (tail + 1U) % SEQ_MAX_TASKS;

        __DMB();

        if (task_fn != NULL)
        {
            task_fn(task_arg);
        }
    }
}
