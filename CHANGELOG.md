# Change log

## 2026-11-19 — Yield the CPU during all SPI/I2C transfers

Refined the migration so every application SPI and I2C data transfer uses an
interrupt/DMA API and a bounded RTOS completion wait. Only the calling task is
suspended while a peripheral transfers data; other ready tasks can execute.
The transfer helper holds no critical section while waiting. This supersedes
the polled EtherCAT transport described in the initial migration entry below.

- Moved EtherCAT SPI4 header and payload exchanges onto `App_SpiTransfer`.
  The 128-byte segment limit, whole-transaction CS assertion and single final
  termination byte remain. Its 2 ms wait now uses RTOS completion notification
  with one extra tick for tick-phase rounding, like other transfer waits.
- Enabled SPI4 IRQ and I2C4 event/error IRQs at priority 6, added their handlers
  and declarations, and recorded the enabled IRQs in CubeMX metadata. The ELF
  checker now verifies these vector targets as well.
- Extended per-bus ownership/completion routing to SPI4 and I2C1/I2C2/I2C4.
  Added a common I2C helper for raw master and memory transfers, with task waits
  and bus-specific recovery after failures. Wired master and memory TX/RX
  completion callbacks to the owner.
- Migrated LED writes, EEPROM access, Hall initialization/legacy read helpers
  and LDC startup/configuration transfers from polling HAL calls to these
  interrupt-driven waits. The normal application paths contain no polled
  SPI/I2C data-transfer calls. IRQ work and brief setup/ownership critical
  sections still consume CPU time; synchronous fault abort/reset cleanup also
  remains bounded and may poll while making hardware safe.
- Retained interrupt-transfer buffer lifetime through completion/abort.
  SPI6 BDMA wire buffers remain aligned in D3 SRAM; Hall DMA remains in AXI
  SRAM. EEPROM I2C4 uses interrupts and does not DMA task-stack buffers.
- Skipped empty EEPROM page operations. An exact page multiple previously
  attempted a zero-length transfer on an extra page; this would violate the
  new transfer helper's nonempty-buffer requirement. Zero-length reads/writes
  now submit no transfer or write-cycle delay.
- Extended the I/O shim tests to assert that every positive completion wait
  starts outside critical sections, exercise deferred SPI4/I2C completions
  after simulated unrelated task work, and check I2C4 timeout/recovery.
  Added an EEPROM suite for exact page multiples, page crossing and zero
  lengths. These tests inspect the API boundary, not real scheduler behavior.
- Updated the runtime guide, test instructions and printable user guide.

Validation: all **five** host suites pass; GCC 13.3.1 Debug, GCC 14.3.1 Debug,
and GCC 14.3.1 charger-enabled Release build; vector/memory checks pass for
all three images; the revised user-guide PDF compiles. Final linker usage:

| Build | DTCM bytes | AXI `.RAM` bytes | D3 bytes | Flash bytes |
| --- | ---: | ---: | ---: | ---: |
| GCC 13.3.1 Debug, batteries ON | 88,752 | 2,120 | 76 | 212,868 |
| GCC 14.3.1 Debug, batteries ON | 88,752 | 2,120 | 76 | 212,980 |
| GCC 14.3.1 Release, batteries OFF | 88,720 | 2,112 | 68 | 144,772 |

Control and EtherCAT still share one task: its next FSM invocation waits for
its own EtherCAT exchange to finish, while other tasks can run. This change
does not establish independent control execution during EtherCAT transfers.
Board measurements must verify the 1 ms cycle and IRQ/wake overhead, along
with the previously listed pulse, DMA/BDMA and fault tests. No board was flashed.

## 2026-11-19 — Initial FreeRTOS migration

The application now runs control/communication, sensor acquisition and services
in statically allocated FreeRTOS tasks. Hardware timers continue to generate
magnetization/demagnetization pulses. This change removes application processing
from peripheral callbacks and establishes one owner per bus and for controller
state. It is implemented and build/host-test validated; it is not yet validated
on the physical controller.

### Kernel and build integration

- Vendored an unmodified subset of **FreeRTOS-Kernel V11.2.0** under
  `Libs/FreeRTOS-Kernel`, including its MIT license and archive provenance/hash.
  Builds need no FreeRTOS download. The existing pinned SOES submodule is not
  modified.
- Added the GCC Cortex-M7 r0p1 port, which also supports later M7 core revisions,
  and integrated kernel sources/headers into maintained CMake targets.
- Added `Core/Inc/FreeRTOSConfig.h`: preemption, 1 kHz kernel tick, six priority
  levels, two notification indexes, static allocation, stack-overflow checks and
  fatal assertions. Kernel dynamic allocation, mutexes and software timers are
  disabled because this architecture does not use them.
- Selected C++17 explicitly. Existing GNU extensions in vendor/application
  headers still produce warnings; this change does not claim a warning-free
  build.
- Added CMake option `EPM_USE_BATTERIES`, default **ON**, preserving the normal
  battery configuration. **OFF** builds charger initialization and periodic
  charger service. `EPM_NUMBER` and existing build presets remain supported.

### Scheduling and startup

- Added `Libs/Runtime/app_runtime.cpp`, `app_runtime.h`, `rtos_io.cpp` and
  `sensor_samples.h` for static task creation, snapshot queues, completion waits,
  ownership and diagnostics.
- `main()` configures the peripherals/trace and starts FreeRTOS. Services runs
  `FootController::init()` after scheduler startup. Worker tasks wait for the
  initialization-ready event before using devices.
- Created five tasks: control (priority 5, 2048-word stack), IMU (4, 1024),
  Hall/LDC acquisition (3, 1024), ToF (2, 2048), services (1, 2048).
  Five application stacks total **32 KiB**, with an additional **1 KiB** idle
  stack. Task control blocks, six single-item queues and the ready event group
  also have static storage.
- TIM2 starts after initialization and posts nominal 1 kHz control/acquisition
  releases. IMU and ToF use data-ready events with periodic level-check fallbacks.
  TIM4 remains configured but is no longer started. IMU report intervals remain
  2500 microseconds.
- Control retains the original **FSM/control before EtherCAT** phase order.
  Delayed releases coalesce; the runtime counts skipped releases and does not
  replay old process-data exchanges. Duplicate pending notifications cannot run
  the same sampled release twice.
- Startup delay calls yield using `App_Delay`. Early startup before scheduler
  start can still use HAL delays. The bounded readiness events replace
  previously unbounded ESC/IMU waits; ToF write-page polling is also bounded.

### Interrupts and time bases

- Added `Core/Src/stm32h7xx_hal_timebase_tim.c`. TIM7 supplies an independent
  1 ms HAL tick; SysTick belongs to FreeRTOS. HAL SPI timeouts therefore continue
  to advance independently of kernel/application processing.
- SVC and PendSV are supplied by the kernel port. SysTick dispatches to the port
  after scheduler startup. TIM7 has its own vector/handler.
- TIM7 uses NVIC priority 4 and makes no RTOS calls; TIM2 uses priority 5; the
  peripheral/EXTI/DMA/BDMA and button/lockout IRQs use priority 6. The RTOS syscall
  ceiling is 5. Existing IRQ priority changes are also recorded in `.ioc`.
- Peripheral callbacks acknowledge hardware or notify the owner. They no longer
  run the FSM, SOES, floating-point telemetry processing, sensor parsing, CRC
  validation or chained application transfers.
- The RTOS/time-base integration is maintained manually. CubeMX regeneration
  must preserve the custom TIM7 file, vector wiring, startup zeroing and MPU
  windows; the `.ioc` alone does not regenerate the complete runtime.

### I/O and sensor drivers

- Added task completion waits with per-bus ownership. Notification index 0 is
  reserved for scheduling/events and index 1 for I/O. Owners are registered before
  enabling transfers, preserving an early completion. Start failure, error and
  timeout paths release ownership only after the hardware is quiesced.
- SPI failures use synchronous HAL abort before returning to callers; abort
  failure is fatal. Hall failures disable I2C2, stop DMA and reinitialize the bus
  including its filters/DMA links. Physical stuck-bus recovery remains a bench
  validation item.
- Hall DMA acquisition sleeps until completion rather than spinning forever.
  Four devices retain order 3,2,1,0. A failed read marks the complete group invalid.
- LDC reads now wait in task context, keeping the local transmit frame alive
  through completion. Slip-filter updates happen after successful reads, and
  validity is cleared before each attempt. Filter members have explicit initial
  values. One LDC is serviced per completed acquisition iteration.
- IMU transactions are serialized. Header and report receives finish before the
  next starts; packet length is checked, multiple known reports are decoded,
  and unknown tails are drained within a bounded packet. Software CS stays
  asserted through the packet. Boot/feature waits yield and time out; the wake
  pin is restored on failure. Message-counter consumption is protected against
  ISR updates.
- Replaced the IMU feature vector with fixed storage. Complete IMU snapshots
  track freshness of enabled acceleration/gyro/quaternion reports; existing
  numeric representation and quaternion acceptance condition are retained.
- ToF interrupt clear and frame reception are performed sequentially by its task.
  Their transmit buffers remain live until completion. Only successful frames
  with the footer validity flag are published.
- Services owns charger SPI6 and CRC. It copies the desired command into a
  separate wire frame, waits for **BDMA** completion and validates received CRC
  before publishing status. Failed transfer/CRC marks the sample invalid and
  retains the last good status.
- LED changes are queued to services; a newer pending animation replaces an older
  one. EEPROM startup write-cycle delays also yield.

### Memory and allocation

- **Preserved `.RAM_D3` for SPI6/BDMA.** Desired/status storage stays there and
  separate wire TX/RX frames are 32-byte aligned in D3 SRAM. BDMA never receives
  a DTCM task-stack buffer. Hall/ADC DMA buffers remain in AXI `.RAM`.
- Preserved the `NOLOAD` sections and added zeroing of both ranges before C++
  constructors. Required nonzero initialization is supplied by constructors or
  transaction setup.
- Added normal, noncacheable, execute-never MPU windows over AXI DMA SRAM and D3
  SRAM. This migration does not enable CPU caches.
- Increased minimum linker heap and interrupt/main-stack reservations to **4 KiB
  each**, independently of task-stack storage. Startup action bindings still use
  the existing C++ heap; the kernel has no dynamic heap.
- Replaced the force-estimation deque with a fixed 20-sample moving average.
  Startup averaging, window length and decay behavior are retained.
- Replaced the FSM state map and copied `std::function` dispatch with member
  function pointers. FSM action bindings remain established during startup;
  cyclic state dispatch does not allocate.

### Commands, EtherCAT and fault behavior

- Control owns pulse requests, lockout state, FSM and `Obj`. Sensor tasks publish
  complete timestamped snapshots through queues instead of modifying fields
  while the control path copies them.
- Physical button hold and TIM6 lockout expiry post control events. TIM1/TIM8
  still generate pulse edges. Existing pulse limits, network command edges and
  dropped-request/no-retry behavior are retained. Startup button-hold events are
  ignored until initialization is ready.
- Clear TIM6's pending update flag before starting each lockout and treat timer
  start failure as fatal. A delayed lockout event conservatively extends lockout.
- Added freshness diagnostics: Hall/LDC 20 ms, enabled IMU reports 50 ms, ToF and
  initialized charger 100 ms. Invalid/never-valid groups are also stale. Warning
  status is raised; SDO `Error_Code` becomes **`0xFF19`** if another warning has
  not taken precedence. This adds no PDO fields and does not inhibit pulses.
- Replaced the incompatible EtherCAT SPI receive pointer with a two-byte buffer
  and explicit AL-event decode. The original firmware **does build with GCC
  13.3.1**; the earlier failure was reproduced with GCC **14.3.1**. The new
  transport builds with both.
- Bounded each polled EtherCAT SPI segment with a 2 ms HAL timeout. Segmented ESC
  reads/writes into at most 128 bytes, holding CS for the complete transaction.
  Only the final read byte terminates it. This fixes the termination-pointer
  overflow for configured 512-byte mailboxes and removes the variable-length
  receive array from writes.
- Bound ESC EEPROM readiness and SOES DL-status startup polling to 2 seconds.
- SOES watchdog decrements now account for elapsed HAL milliseconds rather than
  assuming every cyclic invocation represents exactly 1 ms. PDO processing still
  happens once per invocation; no old exchanges are replayed.
- Installed the SOES safe-output hook. Output loss clears pending network
  requests/control word, disables the desired charger command and asserts
  discharge. It **does not demagnetize**.
- Added a TIM7 monitor that enters the fatal path if a completed control cycle
  has not refreshed its heartbeat for more than 100 ms. This is a software
  progress monitor, not IWDG or proof against every system failure.
- Fatal handling immediately disables gate drivers, stops/disables pulse outputs,
  asserts discharge, clears the stored charger-enable command, disables IRQs and
  halts. It establishes output modes even if initialization was incomplete.
  Cortex fault/NMI handlers and RTOS assertion/stack-overflow hooks use this path.
  It makes no bus/RTOS waits, so a final charger frame and red LED update are no
  longer attempted or guaranteed. Remote charging shutdown still depends on the
  charger's watchdog/board circuitry. Reset remains required after fatal faults.

### Diagnostics and documentation

- Added debugger-visible `app_diagnostics`: release/missed-release counts,
  last/max control duration and maximum wake latency in CPU cycles, control
  heartbeat time, I/O errors, stale-group bits and minimum free task-stack words.
- Added `wiki/RTOS.md` with ownership, configuration, memory, timeouts, failure
  behavior, regeneration instructions and a bench-validation plan.
- Updated architecture, build, configuration, hardware, operation, sensor,
  troubleshooting and development pages and the README/sidebar.
- Updated the standalone user guide to revision B, November 2026, and compiled
  its PDF. The EtherCAT dictionary/PDO mapping and legacy ESI mismatch are
  unchanged. Existing temperature hysteresis/out-of-range behavior, empty
  quick-stop action, parameter-storage limitations and lack of recoverable fatal
  fault reset remain documented.

### Validation performed

- Rebuilt the original baseline with **GCC 13.3.1**, successfully reproducing the
  user's working toolchain. Baseline Debug usage: DTCM 46,440 bytes, AXI `.RAM`
  1,724 bytes, D3 24 bytes, flash 204,936 bytes.
- Built migrated firmware with GCC 13.3.1 Debug/battery ON and GCC 14.3.1
  Debug/battery ON and Release/battery OFF. All link successfully.
- Ran four host suites: FSM transitions and no cyclic allocation; moving-average
  startup/decay; completion/ownership/timeout/recovery; IMU boot and packet
  handling; ESC transfer bounds, AL-event representation and termination.
- Ran `tests/check_firmware.py` on each migrated image to check ARM vectors,
  DTCM task stacks, AXI Hall/ADC buffers and aligned D3 BDMA frames.
- Compiled the updated LaTeX user guide and checked repository documentation
  links and changed-file whitespace (allowing the existing CRLF convention).

Linker-accounted usage for the migrated images is recorded below. It includes
static task stacks and minimum heap/MSP reservations, not measured runtime
stack maxima or peak C++ heap use. These are the repository's configured memory
regions, not a claim about total usable physical MCU SRAM.

| Build | DTCM bytes | AXI `.RAM` bytes | D3 bytes | Flash bytes |
| --- | ---: | ---: | ---: | ---: |
| GCC 13.3.1 Debug, batteries ON | 88,728 | 2,120 | 76 | 215,492 |
| GCC 14.3.1 Debug, batteries ON | 88,728 | 2,120 | 76 | 215,596 |
| GCC 14.3.1 Release, batteries OFF | 88,696 | 2,112 | 68 | 146,180 |

The Debug DTCM figure is approximately **67.7% of 128 KiB**, versus 35.4% at
baseline. Configured D2 SRAM and ITCM remain unused. Compiler/vendor warnings
remain, including existing GNU/C++ initializer extensions and unused parameters.

### Validation still required on hardware

No controller was flashed or exercised during this change. Before commissioning,
measure the 1 ms cycle under simultaneous sensor and maximum mailbox traffic,
release/wake jitter, sensor sample rates, all task stack margins, pulse widths,
100 ms lockout, button behavior, bus loss, sensor disconnections and reconnection,
physical I2C/DMA abort recovery, fatal outputs, cold boot/debug reset and the
optional charger BDMA/CRC path. Host shims do not emulate peripheral cancellation
or scheduler preemption. The software monitor cannot run with IRQs disabled;
there is no newly enabled independent hardware watchdog. See `wiki/RTOS.md` for
commands, diagnostics and the test plan.
