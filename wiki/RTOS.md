# RTOS runtime

[Home](Home.md) · [Architecture](Architecture.md) · [Development](Development.md) · [Change log](../CHANGELOG.md)

The firmware uses a vendored FreeRTOS V11.2.0 kernel with preemptive scheduling
and static kernel allocation. This migration targets bounded asynchronous I/O,
short interrupt handlers, and explicit ownership of application state. It has
been built and checked with host tests; deadlines and peripheral behavior still
require board measurements.

## Tasks and ownership

Larger task priority numbers have greater scheduling priority. Stack sizes below
are **32-bit words**, not bytes.

| Task | Priority | Stack words | Release and responsibility |
| --- | ---: | ---: | --- |
| `control` | 5 | 2048 | TIM2 at nominal 1 kHz; consume snapshots, FSM, pulse requests, EtherCAT |
| `imu` | 4 | 1024 | IMU EXTI; 10 ms fallback; serial receive and report decoding |
| `acquisition` | 3 | 1024 | TIM2; four Hall devices in order 3,2,1,0 and one LDC per completed iteration |
| `tof` | 2 | 2048 | ToF EXTI; 20 ms level-check fallback; interrupt clear and frame read |
| `services` | 1 | 2048 | Startup, then latest LED request, optional charger exchange and stack diagnostics, with at least 10 ms between iterations |
| Kernel idle | 0 | 256 | Scheduler idle work |

Five application stacks reserve 32 KiB; idle adds 1 KiB. TCBs, queues and the
startup event group also have static storage. Kernel dynamic allocation and
software timers are disabled. C++ FSM action bindings still allocate during
startup; cyclic FSM dispatch, IMU feature storage and force filtering do not
allocate. No task uses `printf` or a shared C library allocator at runtime.

The service task initializes the controller after scheduler start. Other tasks
wait on a ready event. TIM2 starts after initialization completes. TIM4 remains
configured but is not started: the IMU is serviced on data-ready events, while
its configured report interval remains 2500 microseconds.

One task owns each peripheral at runtime. Hall devices share one I2C2 owner;
LDC devices share one SPI2 owner. SPI3 belongs to IMU, SPI1 to ToF, SPI6 and CRC
to services, and SPI4/SOES to control. Startup transfers run in services before
ownership is handed over. I2C1 LED writes happen only in services.

## Data exchange and pulse commands

Each sensor group publishes a complete, timestamped value into a single-item
queue. A newer sample replaces an unconsumed older sample. Control receives
these queues without waiting and alone updates `Obj`, the FSM and magnet
request state. Driver receive buffers are never read directly by the PDO copy.

The physical button posts an event to control, preserving its separate
manual-demagnetization path and the existing temperature/lockout/pulse limits.
The timer event does not call the pulse controller from an ISR. Rejected pulse
requests are still dropped rather than queued for retry. TIM6 expiration also
posts an event; any scheduling delay extends lockout conservatively.

TIM1/TIM8 still generate pulse edges. The RTOS does not generate the pulse width.
Before starting TIM6, control clears its pending update flag. The original
control-then-communication order is retained. EtherCAT and control remain in
one task. EtherCAT SPI uses interrupt completion waits: only its caller is
suspended, and other ready tasks can run. The next FSM invocation still waits
for its own EtherCAT exchange because they share a task; no independent control
execution during that exchange is claimed.

## Interrupt and time-base configuration

| Source | NVIC priority | Work |
| --- | ---: | --- |
| TIM7 | 4 | Independent HAL milliseconds and control-progress check; no RTOS calls |
| TIM2 | 5 | Count release and notify control/acquisition |
| Peripheral SPI/I2C/DMA/BDMA/EXTI and TIM5/TIM6 | 6 | Notify owner, record status, or acknowledge timer/button edge |
| SysTick / PendSV | 15 | FreeRTOS tick / context switch |
| SVC | Port controlled | Start scheduler |

Lower NVIC numbers mean greater interrupt urgency. The FreeRTOS syscall ceiling
is 5; every ISR calling a `FromISR` function is at 5 or 6. TIM7 is above that
ceiling and never calls the scheduler. Priority grouping remains
`NVIC_PRIORITYGROUP_4`.

TIM7 supplies the HAL time base both before and after scheduler startup.
SysTick is reserved for the 1 kHz kernel tick. SVC and PendSV are supplied by the
kernel port. Startup delays use `App_Delay`, which yields after the scheduler
starts; its extra tick prevents a requested delay from expiring early.

## I/O completion, bounds and recovery

Scheduling events use task-notification index 0; I/O completion uses index 1.
A transfer registers its owner before enabling the hardware, so a completion
that arrives before the task waits is retained. A task waits rather than spins.
Only one transfer may own a bus slot. Stack-local interrupt-transfer buffers
remain live until the helper returns; **DMA buffers are never on task stacks**.

| Transfer | Completion-wait argument |
| --- | ---: |
| Hall DMA | 5 ms |
| LDC interrupt transfer | 5 ms |
| IMU runtime transfer | 5 ms per stage |
| ToF transfer | 20 ms |
| Charger BDMA | 10 ms |
| EtherCAT interrupt-driven SPI | 2 ms per bounded segment |
| LED / EEPROM interrupt-driven I2C | 10 ms per transfer |

All application SPI and I2C transfers, including startup/configuration and
legacy Hall helpers, start through interrupt or DMA APIs. The wait suspends
only the calling task; it holds no critical section while waiting. Other ready
tasks can run on the CPU while the peripheral works. SPI4 and I2C4 event/error
IRQs are enabled at priority 6. I2C4 uses interrupts, not BDMA or a task-stack
DMA buffer. Task-local buffers remain valid because the helper returns only
after completion or cancellation.

RTOS I/O waits add one kernel tick to the listed arguments for tick-phase
rounding. A failure stops the transfer before releasing the buffer. SPI abort
failure is fatal. I2C error recovery disables the affected peripheral, aborts DMA when used,
and reinitializes that bus including its filters and DMA links. A physically stuck
bus can continue failing; recovery does not promise to repair external wiring
or a slave holding SDA low.

IMU packets are received serially with one CS assertion per packet. Header
lengths are checked, multiple known reports are decoded, and unknown tails are
drained within a 512-byte packet limit. Boot waits and feature-response waits
are bounded. ToF frame reads happen after interrupt-clear completion and only
publish frames whose footer reports validity.

Both the ESC EEPROM-ready wait and the SOES internal DL-status startup polling
have a 2 second bound. ESC reads/writes use segments of at most 128 bytes while
keeping CS asserted; only the last read byte terminates the entire transaction.
This accommodates the configured 512-byte mailboxes without an out-of-bounds
termination pointer or a mailbox-sized local receive array.

## Memory and BDMA

Ordinary static kernel objects and task stacks remain in DTCM. Hall and ADC DMA
buffers remain in `.RAM` at `0x24000000`. Charger desired/status storage and
separate, 32-byte-aligned wire TX/RX frames remain in `.RAM_D3` at `0x38000000`.
The wire frames are passed to SPI6 **BDMA**. They must stay in D3 SRAM; moving
them to a DTCM task stack or AXI SRAM is invalid for this BDMA path.

`.RAM` and `.RAM_D3` remain `NOLOAD`. Startup explicitly zeros both ranges before
C++ constructors run. Constructors and transaction setup supply required field
values. MPU regions mark the complete AXI DMA window and D3 window as normal,
noncacheable, execute-never memory. Caches are not enabled by this migration.
The linker reserves 4 KiB each for the minimum C++ heap and interrupt/main stack;
these reservations are separate from the static task stacks. The C++ heap can
use additional unreserved DTCM through the existing `_sbrk` implementation.

## Watchdogs, freshness and faults

Missed TIM2 releases coalesce: control processes the latest state once and
counts missed releases. It does not replay old PDO exchanges. SOES watchdog
accounting advances once per elapsed HAL millisecond, bounded to 100 decrements
per invocation, rather than assuming every task invocation took exactly 1 ms.
PDO processing is still performed once per control invocation.

The SOES safe-output hook clears pending network commands and the control word,
disables the desired charger command and asserts discharge. It never generates
a demagnetization pulse. The next FSM invocation observes the disabled command.

A TIM7 progress monitor invokes the fatal path when a completed control cycle
has not updated its heartbeat for **more than 100 ms**. This is a software
monitor, not an independent hardware watchdog: it cannot run with interrupts
globally disabled and it does not detect every failure of individual tasks.
It remains active if the scheduler stalls while TIM7 can still execute.

Fatal handling disables gate drivers, stops/disables both pulse timers,
asserts discharge, clears the stored charger enable command, disables IRQs and
halts. It makes no blocking bus calls and does not try to update the LED or
transmit a final charger frame. Remote charger shutdown consequently also
depends on its own watchdog and board circuitry. A fatal halt still requires
reset; recoverable fault-reset behavior has not been introduced.

Freshness checks use these maximum sample ages:

| Group | Maximum age |
| --- | ---: |
| Complete Hall group | 20 ms |
| LDC group; every channel must have succeeded | 20 ms |
| IMU; enabled linear acceleration, gyro and quaternion reports | 50 ms |
| ToF valid frame | 100 ms |
| Charger, when initialized | 100 ms |

A never-valid, invalid or over-age sample sets its bit in
`app_diagnostics.sensor_stale_mask`. Existing PDO values remain available but
must not be treated as fresh. Warning status is raised, and `Error_Code` at
SDO `0x603F:00` becomes `0xFF19` if another existing warning has not taken
precedence. PDO mappings and lengths are unchanged. Freshness is not a new
pulse inhibit. Existing temperature branch behavior and empty quick-stop
behavior remain documented limitations.

## Diagnostics and validation

Inspect `app_diagnostics` in a debugger: release/miss counters, last/max control
cycle duration, maximum wake latency (DWT CPU cycles), last completed-control
HAL timestamp, I/O errors, stale mask and each task's minimum remaining stack
in words. The wake latency records the latest release when events coalesce;
use the missed-release counter alongside it.

Run host tests separately from the embedded build:

```sh
cmake -S tests -B /tmp/magnecko-tests -G Ninja
cmake --build /tmp/magnecko-tests
ctest --test-dir /tmp/magnecko-tests --output-on-failure
python3 tests/check_firmware.py build/Debug/MagneckoFoot_Header.elf
```

Host tests cover FSM transitions/no cyclic allocation, moving-average startup
and decay, early completion/timeout/error/recovery/ownership, bounded IMU boot
and packet handling, ESC transfer boundaries/termination, deferred waits outside critical sections,
and EEPROM page boundaries/zero-length requests. The ELF checker
checks vector targets (including SPI4 and I2C4) and task/DMA/BDMA memory placement. Mocked HAL tests do
not emulate peripheral timing or demonstrate scheduler behavior on silicon.

Before commissioning, measure pulse widths/lockout, worst-case 1 ms cycle and
wake latency with simultaneous sensor and 512-byte mailbox traffic, acquisition
rates, all stack minima, bus-loss behavior, button requests, sensor-disconnect
recovery, fatal outputs and the optional charger/BDMA path. Check cold startup
and debugger resets. Do not infer deadline compliance from a successful build.

## CubeMX regeneration

The RTOS is integrated in maintained CMake/runtime files, not generated by
CubeMX middleware. The `.ioc` records the new priorities for existing IRQs;
it does not generate the custom TIM7 HAL time-base integration. After
regeneration retain `stm32h7xx_hal_timebase_tim.c`, its CMake entry, the custom
SVC/PendSV/SysTick wiring, TIM7 priority/handler, MPU windows and startup zeroing.
Do not add another generated time-base file or another kernel. Review the
C++ entry-point conversion as described in [Development](Development.md).
