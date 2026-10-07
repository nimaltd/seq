# 🌀 seq

[![CI](https://github.com/nimaltd/seq/actions/workflows/ci.yml/badge.svg)](https://github.com/nimaltd/seq/actions/workflows/ci.yml)
[![Stars](https://img.shields.io/github/stars/nimaltd/seq?style=social)](https://github.com/nimaltd/seq)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE.md)

A small non blocking state sequencer with a built in task queue, written in C for STM32.

The point of this library is to get `HAL_Delay()` out of your main loop. States change after a delay without blocking, and interrupts hand their work to a queue instead of doing it inside the handler. Your main loop stays responsive and your ISRs stay short.

It is around 100 lines of actual code, it needs no RTOS, and it works on any STM32 family.

---

## ✨ What you get

- Non blocking state transitions with millisecond resolution
- A lock free task queue that is safe to use from an interrupt, and carries
  an argument, so an interrupt can say which peripheral the work is for
- One set of state functions can drive several machines, since each state is
  handed the handle it belongs to
- A burst of queued work clears in one pass, without ever starving your sequence
- No dynamic memory, no RTOS, no dependencies beyond the HAL tick
- Unit tested on every commit

---

## 📁 Layout

```
src/    seq.h, seq.c, seq_config.h
test/   host unit tests, run on a PC
```

Installed into a project, the code keeps its `src/` folder, with your `seq_config.h`
beside `seq.h`, and the README, changelog and licence files around it. There is no
`test/`: the section below about the tests refers to this repository, not to an
installed copy.

---

## ⚙️ Installing it

[stm32-installer](https://github.com/nimaltd/stm32-installer) copies the library into your project, `seq_config.h` included, and adds it to your CMake, STM32CubeIDE, Keil, IAR or Makefile project for you. Your project file is backed up first.

Install it once per machine:

```bash
pip install stm32-installer
```

Then, from the root of your STM32 project:

```bash
stm32-installer nimaltd/seq
```

### From a downloaded zip

Downloaded this repository with **Code**, **Download ZIP**? Give the installer the zip in place of `nimaltd/seq`, with no need to unpack it:

```bash
stm32-installer D:/Downloads/seq-main.zip
```

Only the files the library needs are copied into your project, and the zip is left alone. An unpacked folder works the same way. [stm32-installer's README](https://github.com/nimaltd/stm32-installer#installing-a-library) has every option, and how to install on a machine with no internet at all.

### Updating, and pinning a version

Run the same command again. Every file is replaced, and your settings in `seq_config.h` are kept: whatever is between `USER CODE BEGIN SEQ_CONFIGURATION` and `USER CODE END SEQ_CONFIGURATION`. Anything you changed outside those lines is saved to a `.bak` file first. This needs stm32-installer 1.9.0 or newer. An older one cannot find the library: update it with `pip install --upgrade stm32-installer`.

By default you get the newest code on `main`. To hold a project on one release, add `--ref` with a tag, a branch or a commit:

```bash
stm32-installer nimaltd/seq --ref v2.0.0
```

### Or copy the files in by hand

1. Copy `src/seq.h` into your project's `Core/Inc`
2. Copy `src/seq.c` into your project's `Core/Src`
3. Copy `src/seq_config.h` into `Core/Inc`

Keep your settings between its `USER CODE BEGIN` and `USER CODE END` lines. Installing later with stm32-installer keeps what is between them and replaces the rest.

### Or add the whole repository to a CMake build

If you keep this repository as a submodule rather than installing it:

```cmake
add_subdirectory(seq)
target_link_libraries(${CMAKE_PROJECT_NAME} nimaltd::seq)

# seq is a static library, so it does not inherit your application's include
# paths and defines, and seq.c needs main.h and the HAL. A CubeMX project
# keeps them on the stm32cubemx target.
target_link_libraries(seq PRIVATE stm32cubemx)
```

The first `target_link_libraries` has no `PRIVATE` on purpose. CubeMX links your application without one, and CMake refuses to mix the two forms on one target. The settings come from `seq/src/seq_config.h`, beside `seq.h`.

`stm32-installer` avoids all of this: it writes an INTERFACE target instead, whose sources compile as part of your own target and inherit everything it has.

---

## 🔧 Configuration

Everything lives in your `seq_config.h`, between its `USER CODE` lines, which every install keeps:

```c
#define SEQ_MAX_TASKS       16U
```

One slot is always kept free so a full queue can be told apart from an empty one, so `16` gives you room for 15 queued tasks. The minimum is 2.

---

## 🚀 Getting started

```c
#include "seq.h"

seq_t my_seq;

void state_idle(seq_t *seq, void *arg)
{
    if (something_happened())
    {
        seq_next(seq, state_measure, NULL, 0);
    }
}

void state_measure(seq_t *seq, void *arg)
{
    start_measurement();

    /* Come back in 200 ms, without blocking anything. */
    seq_next(seq, state_report, NULL, 200);
}

void state_report(seq_t *seq, void *arg)
{
    send_result();
    seq_next(seq, state_idle, NULL, 0);
}

int main(void)
{
    /* ... HAL init ... */

    seq_init(&my_seq, state_idle, NULL);

    while (1)
    {
        seq_loop(&my_seq);
    }
}
```

Each state is given the handle it belongs to, so it never has to name the machine
it is running on. That is what lets the same three functions run four sensor
channels: four handles, one set of states.

### Giving a machine something of its own

Put the `seq_t` first in a struct of your own, and cast the handle back to it
inside the state:

```c
typedef struct
{
    seq_t               seq;      /* must be the first member */
    UART_HandleTypeDef *uart;
    uint8_t             address;
    int32_t             result;
} channel_t;

channel_t channels[4];

void state_measure(seq_t *seq, void *arg)
{
    channel_t *channel = (channel_t *)seq;

    channel->result = start_measurement(channel->uart, channel->address);
    seq_next(seq, state_report, NULL, 200);
}

void state_report(seq_t *seq, void *arg)
{
    channel_t *channel = (channel_t *)seq;

    send_result(channel->result);
    seq_next(seq, state_measure, NULL, 1000);
}

int main(void)
{
    for (int i = 0; i < 4; i++)
    {
        seq_init(&channels[i].seq, state_measure, NULL);
    }

    while (1)
    {
        for (int i = 0; i < 4; i++)
        {
            seq_loop(&channels[i].seq);
        }
    }
}
```

The cast is safe because C guarantees that a pointer to a struct and a pointer
to its first member point at the same place. Use this for what belongs to a
machine for its whole life. For what one state hands the next, there is `arg`.

### Handing something to the next state

The argument you give `seq_next()` right after the state is handed to that
state, on every run, until the next transition replaces it:

```c
void state_measure(seq_t *seq, void *arg)
{
    static int32_t result;

    result = read_sensor();
    seq_next(seq, state_report, &result, 0);
}

void state_report(seq_t *seq, void *arg)
{
    int32_t *result = arg;

    send_result(*result);
    seq_next(seq, state_measure, NULL, 1000);
}
```

Only the pointer travels, not what it points at, so that has to outlive the
wait. A `static`, a global, or a field of your own struct is fine. The address
of a local variable is not: the state that called `seq_next()` has long returned
by the time the next one runs.

A small number fits too, without pointing at anything:

```c
void state_idle(seq_t *seq, void *arg)
{
    if (button_pressed())
    {
        seq_next(seq, state_blink, (void *)(uintptr_t)3, 0);
    }
}

void state_blink(seq_t *seq, void *arg)
{
    uint32_t times = (uint32_t)(uintptr_t)arg;

    blink_led(times);
    seq_next(seq, state_idle, NULL, 0);
}
```

`seq_init()` takes one as well, for the state a machine starts in.

### Doing something once when a state starts

A state runs on every `seq_loop()` until it moves on. For work it should do only
once, such as sending a request before it waits for the answer, ask
`seq_first_run()`:

```c
void state_wait_for_reply(seq_t *seq, void *arg)
{
    if (seq_first_run(seq))
    {
        send_request();
    }

    if (reply_ready())
    {
        seq_next(seq, state_use_reply, NULL, 0);
    }
    else if (seq_time(seq) >= 500)
    {
        seq_next(seq, state_no_reply, NULL, 0);
    }
}
```

Every transition counts as a new start, one back into the same state included,
and after a wait the first run is the one the wait ends with. Checking
`seq_time()` for 0 is not the same thing: the main loop can go round many times
in one millisecond, and `seq_time()` reads 0 on every one of them.

### Handing work over from an interrupt

This is the part that keeps your ISRs honest. The handler queues a function and returns immediately, and the work itself runs later from the main loop.

```c
void button_pressed(void *arg)
{
    /* Runs from seq_loop(), so you can take your time here. */
    (void)arg;
    read_sensor();
    update_display();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == B1_Pin)
    {
        seq_task_add(button_pressed, NULL);
    }
}
```

The argument is there for when one task serves several sources. A UART callback
already knows which port it is:

```c
void handle_line(void *arg)
{
    UART_HandleTypeDef *uart = arg;

    parse_and_reply(uart);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    seq_task_add(handle_line, huart);
}
```

Only the pointer is copied into the queue, not what it points at, so it has to
outlive the call. A peripheral handle or a static buffer is fine. The address of
a local variable in the interrupt handler is not.

### What is safe to call from an interrupt

`seq_task_add()`, and nothing else.

Everything else expects to be called from the same place as `seq_loop()`, normally the main loop. `seq_next()` and `seq_stop()` each write several fields of the handle, and an interrupt landing in the middle leaves it half updated, most often with the new state set but not yet its wait or its argument, so it runs at once, with the argument meant for the state before it. Nothing reports this, and it only happens on the timing where the interrupt lands badly, which is the worst kind of bug to chase.

So changing state from an interrupt looks like this, not like a direct call:

```c
static void on_button(void *arg)
{
    seq_next(arg, state_pressed, NULL, 500); /* main loop, safe */
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    seq_task_add(on_button, &my_seq);        /* the interrupt only hands over */
}
```

---

## 🧰 API

| Function | What it does |
|---|---|
| `void seq_init(seq_t *handle, seq_state_fn_t first_fn, void *arg)` | Set up a handle, the state it starts from, and that state's argument |
| `void seq_loop(seq_t *handle)` | Run queued tasks and the current state. Call it from your main loop |
| `void seq_next(seq_t *handle, seq_state_fn_t next_fn, void *arg, uint32_t wait_ms)` | Choose the next state and its argument, and how long to wait before it runs, without blocking |
| `uint32_t seq_time(const seq_t *handle)` | How long the machine has been in the current state |
| `bool seq_first_run(const seq_t *handle)` | True during the first run of the current state, for work it does once when it starts |
| `void seq_stop(seq_t *handle)` | Halt the machine. Nothing runs until the next `seq_next()` |
| `bool seq_running(const seq_t *handle)` | False once stopped |
| `seq_err_t seq_task_add(seq_task_fn_t task_fn, void *arg)` | Queue a task with its argument. Safe to call from an interrupt |
| `uint32_t seq_task_peak(void)` | The most tasks ever queued at once |
| `void seq_task_flush(void)` | Drop everything queued. Fine from the main loop or from a task, not from an interrupt |

`seq_task_add()` returns `SEQ_ERR_NONE` when the task was queued, or `SEQ_ERR_FULL` when the queue is full. It is the only call that is safe from an interrupt, and `seq_loop()` must have a single caller, since two would both consume the queue and could take the same task twice.

A `NULL` pointer is a bug in the calling code, so no function returns it as an error: `assert_param()` stops at it, the way the HAL does. That needs **Enable Full Assert** in CubeMX (Project Manager, Code Generator), which defines `USE_FULL_ASSERT`. Turn it on while developing, and a `NULL` lands in `assert_failed()` with the file and line. Without it nothing checks, except that a `NULL` task is skipped rather than called. `SEQ_ERR_INVALID` is still defined, so code that names it compiles, but nothing returns it.

Use `seq_task_peak()` to size `SEQ_MAX_TASKS` by measurement. A full queue is reported to the caller, but that caller is usually an interrupt handler where nobody checks a return value, so the peak is in practice the only way to find out you were close to overflowing.

Stopping does not stop the task queue. Tasks belong to the application rather than to any one machine, so `seq_loop()` keeps serving them even on a stopped machine.

Each `seq_loop()` runs the tasks that were queued when it began, so a burst clears in one pass instead of one task per iteration. Anything queued while those tasks run, including by a task queueing another, waits for the next pass. That is deliberate: draining until the queue is empty would never return if a task kept requeueing itself, or if an interrupt produced faster than the loop could drain, and your states would stop running.

---

## 🧪 Running the tests

The tests run on your PC, not on hardware. Time is faked, so a 200 ms delay is tested instantly. You need cmake and any C compiler, nothing else: [Unity](https://github.com/ThrowTheSwitch/Unity) is vendored into `test/unity/`, so there is nothing to install.

One command does everything:

```bash
python test/run_tests.py
```

It configures, builds and runs the suite, then tells you plainly whether it passed. Add `--clean` to start from an empty build folder.

If you prefer doing it by hand:

```bash
cmake -S . -B build -DSEQ_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## ⬆️ Coming from version 1

Three things moved:

- The files now live in `src/` instead of the repository root
- `seq_config.h` now ships in `src/`, and your settings go between its `USER CODE` lines
- `seq.h` no longer includes `main.h`. If a file of yours relied on that, include `main.h` yourself

Your state and task functions need one change each. A state now takes the handle it belongs to and the argument it was entered with, and a task takes the argument it was queued with:

```c
void my_state(void);                     /* was */
void my_state(seq_t *seq, void *arg);    /* is now */

void my_task(void);                      /* was */
void my_task(void *arg);                 /* is now */
```

`seq_init()`, `seq_next()` and `seq_task_add()` each gained an argument, always directly after the function it is handed to, so in `seq_next()` it comes before the wait. Pass `NULL` wherever there is nothing to hand over.

The compiler finds every one of these for you, which is the point of doing it this way rather than leaving the old shape available beside the new one.

---

## 🤝 Contributing

Bug reports and pull requests are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for the style rules and how to run the tests. Nothing to sign, just open a pull request.

---

## 💖 Support

I write these libraries in my own time and give them away, because good tools should be easy to get. If this one saved you an afternoon, there are two things that genuinely help:

**⭐ Star the repo.** It costs you one click, it helps other engineers find the library, and it is the main reason I keep going.

**☕ [Buy me a coffee on Ko-fi](https://ko-fi.com/nimaltd).** Any amount is a real motivation to keep writing, documenting and maintaining this work.

[![GitHub](https://img.shields.io/badge/GitHub-Follow-black?style=for-the-badge&logo=github)](https://github.com/NimaLTD)
[![YouTube](https://img.shields.io/badge/YouTube-Subscribe-red?style=for-the-badge&logo=youtube)](https://youtube.com/@nimaltd)
[![Instagram](https://img.shields.io/badge/Instagram-Follow-purple?style=for-the-badge&logo=instagram)](https://instagram.com/github.nimaltd)
[![LinkedIn](https://img.shields.io/badge/LinkedIn-Connect-blue?style=for-the-badge&logo=linkedin)](https://linkedin.com/in/nimaltd)
[![Email](https://img.shields.io/badge/Email-Contact-red?style=for-the-badge&logo=gmail)](mailto:nima.askari@gmail.com)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-Support-orange?style=for-the-badge&logo=ko-fi)](https://ko-fi.com/nimaltd)

---

## 📜 License

Apache License 2.0. See [LICENSE.md](LICENSE.md).

You are free to use this in commercial and closed source products. What the license asks in return is that you keep the copyright notice and pass along the [NOTICE](NOTICE) file, so the credit travels with the code.

The test folder vendors [Unity](https://github.com/ThrowTheSwitch/Unity) under its own MIT license, kept in [test/unity/LICENSE.txt](test/unity/LICENSE.txt). It is only used for testing and is not part of what you flash to a device.
