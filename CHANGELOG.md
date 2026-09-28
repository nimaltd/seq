# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.1] - 2026-09-28

### Changed

- **Renamed from `sequencer` to `seq`**, the name its files and functions
  already carry, like the other NimaLTD libraries. GitHub sends the old address
  to the new one. Install it as `stm32-installer nimaltd/seq`, and it lands in a
  folder called `seq`.
- Installed with stm32-installer, the code now keeps its `src/` folder, with your
  `seq_config.h` beside `seq.h`, instead of every file sitting at the top of one
  folder. Nothing in the code changed.
- Needs stm32-installer 1.3.0 or newer. Updating an earlier install moves your
  `seq_config.h` into `src/` with your settings in it, removes the old copies of
  `seq.h` and `seq.c`, and points Keil, IAR and a CubeMX Makefile at the new
  places. An older installer stops and says to update, rather than leave both
  copies of `seq.c` in the project.

## [2.0.0] - 2026-09-24

### Changed

- **Renamed from `fsm` to `sequencer`.** The old name promised a finite state
  machine, and this is not one: there is no declared set of states, no events,
  and no transition table. It is a non blocking sequencer for callbacks with
  delays, plus a task queue for interrupts, and the name now says so.
- Every public name changes with it, so code written against `fsm` will not
  compile until it is updated. The mapping is one for one:

  | Was | Is now |
  |---|---|
  | `fsm.h`, `fsm.c`, `fsm_config.h` | `seq.h`, `seq.c`, `seq_config.h` |
  | `fsm_t`, `fsm_err_t` | `seq_t`, `seq_err_t` |
  | `fsm_fn_t` | `seq_state_fn_t` and `seq_task_fn_t` |
  | `fsm_init`, `fsm_loop`, `fsm_next`, `fsm_time` | `seq_init`, `seq_loop`, `seq_next`, `seq_time` |
  | `fsm_task_add` | `seq_task_add` |
  | `FSM_MAX_TASKS`, `FSM_ERR_NONE`, `FSM_ERR_FULL` | `SEQ_MAX_TASKS`, `SEQ_ERR_NONE`, `SEQ_ERR_FULL` |

  A find and replace of `fsm_` to `seq_` and `FSM_` to `SEQ_` covers all of it.

- Sources moved to `src/`, flat, with the configuration beside them.
- `seq.h` no longer includes `main.h`. Include it yourself if you relied on that.
- **State functions and tasks now take parameters.** A state is handed the
  handle it belongs to and the argument it was entered with, and a task is
  handed whatever it was queued with:

  ```c
  void seq_init(seq_t *handle, seq_state_fn_t first_fn, void *arg);
  void seq_next(seq_t *handle, seq_state_fn_t next_fn, void *arg, uint32_t wait_ms);
  seq_err_t seq_task_add(seq_task_fn_t task_fn, void *arg);

  void my_state(seq_t *handle, void *arg);
  void my_task(void *arg);
  ```

  Without these, a state has to name its machine through a file scope variable,
  one state cannot hand anything to the next except through a global, and an
  interrupt cannot say which peripheral its work belongs to. None of the three
  needs a global now. Pass `NULL` wherever there is nothing to hand over.

  For data that belongs to a machine for its whole life, make `seq_t` the first
  member of your own struct and cast the handle back to it inside the state.

- The wait before a state runs is now called `wait_ms`, in `seq_next()` and in
  `seq_t`. In STM32 code "delay" reads as `HAL_Delay()`, which blocks, and not
  blocking is the whole point here. Calls do not change, since a parameter name
  never appears at a call site.

- The single `fsm_fn_t` became `seq_state_fn_t` and `seq_task_fn_t`, because a
  state and a task are not the same thing and no longer have the same shape.
- Licence changed to Apache-2.0.

### Added

- `seq_stop()` and `seq_running()`, to halt a sequence and ask whether it is halted.
- `SEQ_ERR_INVALID`, returned by `seq_task_add()` when the task pointer is NULL, so a bad argument is no longer reported as a full queue.
- `seq_task_peak()` and `seq_task_flush()`, to size `SEQ_MAX_TASKS` by measurement and to drop queued work. `seq_task_flush()` is fine from inside a task as well as from the main loop.
- `seq_first_run()`, true during the first run of a state, for work the state does only once, such as sending a request before it waits for the answer. Checking `seq_time()` for 0 cannot stand in for it, since the main loop can go round many times in one millisecond.
- Host unit tests, run with `python test/run_tests.py`.
- CMake build, and a `library.yml` for installing with stm32-installer, from
  GitHub or from a downloaded zip.

### Fixed

- `seq_time()` returned zero inside a running state, so every timeout built on it silently never fired.
- A queued task could be lost when two interrupts called `seq_task_add()` at the same moment. `SEQ_ERR_NONE` was returned for both.
- `seq_time()` now returns 0 while the sequence is stopped, rather than a number that keeps climbing for a state nothing is running.
- `seq_loop()` now clears a whole burst of queued tasks in one pass rather than one per iteration, so the last task of a burst no longer waits behind every task ahead of it. It stops at the queue's end as it was on entry, so a task that queues more work cannot hold the loop and starve the states.
- `SEQ_MAX_TASKS` below 2 is refused at compile time. One slot is always left free, so a smaller queue could never accept anything, and it failed silently.
