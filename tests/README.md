# Firmware host tests

These tests compile selected production sources against a small HAL/RTOS shim.
They do not emulate the FreeRTOS scheduler or an STM32 peripheral.

```sh
cmake -S tests -B /tmp/magnecko-tests -G Ninja
cmake --build /tmp/magnecko-tests
ctest --test-dir /tmp/magnecko-tests --output-on-failure
python3 tests/check_firmware.py build/Debug/MagneckoFoot_Header.elf
```

- `controller_logic`: enable/disable/quick-stop/fault transitions, no allocations
  during repeated FSM dispatch, force-window startup and complete decay.
- `rtos_io`: early ISR completion, per-bus ownership rejection, error/start
  failure/timeout handling, abort/reset and subsequent successful transfers,
  late notifications with no owner, selection of the DMA transfer API, deferred SPI4/I2C completions and checks
  that positive waits happen outside critical sections. Simulated other-task
  work runs before the deferred completion; this is not a scheduler emulator.
- `imu_transactions`: SPI clock priming during reset, variable boot packet
  lengths/order, missed and stale interrupts, per-command wake handshake,
  full-duplex packet draining, matched
  and bundled feature acknowledgements, bounded missing-device/response waits,
  complete packets with multiple reports, malformed lengths and failed transfers,
  status-bit quality checks, quaternion validity, tick rollover, reset/re-init,
  and CS/WAKE cleanup.
- `esc_transport`: zero, near-boundary and multi-segment read/write sizes through
  513 bytes, byte order of the AL event, bounded segments, one CS transaction,
  and one final read-termination byte.
- `eeprom_transactions`: exact pages, page crossing and zero-length operations
  do not submit empty I2C transfers.
- `check_firmware.py`: linked ARM vectors including SPI4/I2C4, static task-stack placement, Hall/ADC
  DMA placement and D3 placement/alignment of BDMA wire frames. This uses only
  Python's standard library and reads the ELF directly.

The test shim intentionally lacks most HAL behavior. Real DMA cancellation,
IRQ preemption, scheduler wake latency, external sensor framing and electrical
recovery still require the bench checks in [RTOS runtime](../wiki/RTOS.md).

For BNO086 startup changes, repeat both cold power cycles and MCU-only resets.
Capture NRST, WAKE, H_INTN, CS and SCLK to verify that WAKE stays high during
boot, the clock reaches its mode-3 idle level before reset is released, and each
feature command starts after H_INTN goes low. Check that all three enabled
reports become fresh and keep arriving without batching delays.
