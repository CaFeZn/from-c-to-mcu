# Serial HAL and Flow-Control Examples

These examples accompany [HAL callbacks](../../../../docs/notes/hal-callbacks.md) and [flow control / USB CDC](../../../../docs/notes/flow-control-usb-cdc.md).

## Host models

- [callback_model.c](callback_model.c) models selected STM32F4 HAL 1.8.5 callback registration states, per-handle dispatch, one-shot receive completion, IRQ connectivity, and the normal DMA-TX -> UART-TC sequence.
- [flow_model.c](flow_model.c) models asynchronous buffer ownership, two four-byte queue slots, retry after FULL/BUSY, and a 16-byte receive ring with high/low watermarks 12/4 and at most three trailing bytes after stop.

Both are deterministic, single-threaded C99 programs with assertions and fixed output. They do not compile or emulate HAL, USB, electrical signals, real interrupt races, USB disconnects, or actual device timing. Assertions must remain enabled; use the Debug configuration below.

Run from the repository root with a discovered host compiler and CMake generator:

```sh
cmake -S examples/notes/serial/hal-flow -B artifacts/course-completion/serial/hal-flow/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build artifacts/course-completion/serial/hal-flow/build
ctest --test-dir artifacts/course-completion/serial/hal-flow/build --output-on-failure
```

On this workstation, command discovery returned GCC at C:/MinGW/bin/gcc.exe (6.3.0), CMake and Ninja on PATH. Configuration was run with an explicit CMAKE_C_COMPILER=C:/MinGW/bin/gcc.exe. Do not assume that location on another machine.

Expected final lines are `callback model: PASS` and `flow model: PASS`. The flow model reports 29 consumed / 11 dropped with watermarks off and 40 consumed / 0 dropped with watermarks on; peak occupancy is 16 and 14 respectively. Queue output is ABCDEFGHIJKL. The XBCD line is an intentional serialized buffer-reuse counterexample, not an undefined-behavior experiment.

## STM32F407 board probe

[hal_callback_probe.c](hal_callback_probe.c) and [hal_callback_probe.h](hal_callback_probe.h) are integration fragments only. They were not built as firmware or run on a board.

Prerequisites:

- A working STM32F407 CubeMX project with STM32F4 HAL, UART enabled, initialized USART2, its actual GPIO alternate functions, and a valid clock configuration.
- Generated usart.h declaring huart2. If the project keeps the declaration elsewhere, adapt that include rather than inventing another handle.
- A matching HAL configuration in every translation unit. USE_HAL_UART_REGISTER_CALLBACKS must be 0 or 1; changing it changes UART_HandleTypeDef layout, so rebuild all dependent objects.
- USART2 IRQ enabled in NVIC and the existing USART2_IRQHandler calling HAL_UART_IRQHandler(&huart2).
- Correct, common-ground, 3.3 V-compatible logic-level serial wiring. No RS-232 levels. Use the board's actual mapped RX pin.

Integration:

1. Add the probe source/header to user-owned source/include directories and the build source list. They are deliberately excluded from the host-model CMake targets.
2. Include hal_callback_probe.h at the caller. After MX_USART2_UART_Init and before any UART transfer, call callback_probe_start and check HAL_OK; arrange error handling for any other return value.
3. With the macro equal to 1, the probe registers its per-handle receive callback. With 0, it supplies HAL_UART_RxCpltCallback instead. Do not add a duplicate strong callback already owned by another module; merge the dispatch there.
4. Disable terminal newline suffixes. Send AB, then CD: callback_probe_hits is expected to stay 0 then become 1. The static probe_rx buffer is available to a debugger. Only after completion, call start again and send EFGH to expect 2.
5. This is a one-shot probe, not a production receiver. It has no queue, automatic rearm, error recovery or concurrent start protection. Pause traffic before debugging; reset between configuration variants.

The volatile counter is an observation aid, not a synchronization protocol. No board, USB enumeration, USB traffic, or electrical test was performed.

## Evidence

See the [local review](../../../../artifacts/course-completion/serial/hal-flow/review.md) for exact source revisions, commands, results and limitations. Official source caches are read-only evidence under that report's references directory; they are not linked into these models.
