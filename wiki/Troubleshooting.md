# Troubleshooting and known limitations

[Home](Home.md) · [Operation](Operation.md) · [Build and flash](Build-and-Flash.md)

## Error codes

Read `Error_Code` with SDO **`0x603F:00`**; it is not in the default TxPDOs. Status-word warning bit 7 is a separate indication. The background callback resets and rebuilds the error code on each call. Multiple checks may overwrite an earlier code, and a fatal halt can prevent the last code from reaching the master.

| Code | Meaning / implementation |
| --- | --- |
| `0x0000` | No currently reported error |
| `0x2220` | General overcurrent enum; not raised by the current controller background logic |
| `0x4300` | Overtemperature warning or fault depending on threshold |
| `0xFF01`–`0xFF0F` | Sensor initialization failures combined using low-nibble bits: IMU `1`, LDC `2`, ToF `4`, Hall `8` |
| `0xFF10` | Timer initialization failure; sets fault bit but does not call the fatal reaction itself |
| `0xFF11` | Invalid EEPROM parameters; validation is currently a stub |
| `0xFF12` | Charger overcurrent |
| `0xFF13` | Charger overvoltage |
| `0xFF14` | Charger watchdog fault |
| `0xFF15` | Charger did not initialize/respond |
| `0xFF16` | Temperature sensor disconnected/out of range |
| `0xFF17` | Gate-driver fault while gate drivers are enabled |
| `0xFF18` | Invalid simultaneous input requests detected by the controller action |
| `0xFF19` | Missing, invalid or stale sensor group; see `app_diagnostics.sensor_stale_mask` and [RTOS limits](RTOS.md) |

For example, `0xFF05` combines IMU and ToF initialization failure; `0xFF0F` indicates all four sensor groups. The ordinary command callback cancels simultaneous rising requests before the operation action sees them, so `0x03` need not produce `0xFF18`.

## Symptoms and checks

| Symptom | What to inspect |
| --- | --- |
| CMake cannot find ARM compiler or Ninja | Put the toolchain binaries on `PATH`; select a CMake preset |
| Missing `SOES/soes/*.c` | Run `git submodule update --init --recursive`; check SSH access or local HTTPS override |
| GCC 14 fails in `esc_address()` | Check for a stale pre-migration checkout; current transport uses a two-byte receive buffer |
| Startup remains in configuration pattern | Inspect `EEPROM_LOADED` and external ESC reset/power; EEPROM-ready and DL-status waits are bounded to 2 seconds |
| Master PDO lengths or entries are unexpected | Current XML is stale; compare compiled C dictionary and actual EEPROM/SII |
| PREOP → SAFEOP fails with DC enabled | No distributed-clock check handler is configured; use a polling-compatible configuration |
| Master is OP but pulse commands do nothing | Check masked application state, command rising edges, entry-time request clearing, lockout, and temperature flags |
| Repeated identical command is ignored | Send zero long enough to be consumed before raising the bit again |
| A request within 100 ms is lost | The lockout drops requests; retry with a fresh edge after more than 100 ms and any required recharge interval |
| Capacitor voltage is always zero | Default `USE_BATTERIES` leaves charger uninitialized; zero is not a verified measurement |
| Force estimate is zero | Inspect software magnet flag, Hall initialization/values, and three-of-four LDC slip votes |
| Force briefly persists after demagnetizing | The moving average retains old samples until they leave its 20-sample window |
| A sensor value stops changing | Sensor PDOs lack freshness/validity flags; inspect driver state, interrupts, and transfer completion |
| Communication stopped and outputs disabled | Fatal handler or control-progress monitor halted; inspect debugger state, correct cause and reset; LED update is not guaranteed |
| Fault reset has no effect | Fatal handler does not reach a serviceable generic FSM fault state |
| Button does not release magnet | Check hold duration, dead time, temperature flag, available pulse energy, and whether MCU is halted |

## Toolchain difference

The pre-migration checkout builds with GCC 13.3.1 (the editor's configured
compiler). GCC 14.3.1 rejects its incompatible SPI receive-buffer pointer at
`esc_address()`. The RTOS migration uses a two-byte receive buffer and explicit
AL-event decoding and has been built with both toolchains. This earlier
observation did not establish a failure with the user's GCC 13 build.

## Integration limitations to retain when modifying the code

1. **EtherCAT artifact mismatch:** XML and C dictionary disagree, and JSON identity metadata differs too. Reconcile artifacts and actual SII before commissioning.
2. **Fault policy:** the SOES safe-output hook inhibits network commands, disables desired charging and asserts discharge without demagnetizing. Quick-stop action remains empty; it is not a release procedure.
3. **Fatal fault recovery:** fatal handling disables pulses/gate drivers, asserts discharge and halts without I/O waits. It cannot transmit a final fault/charger frame or guarantee a red LED animation. Reset remains required.
4. **Temperature behavior:** PT1000 out-of-range is warning-only; stale exported temperature is possible. A 65–80 °C branch clears the overtemperature flag, so complete 80/50 °C hysteresis is not implemented.
5. **Parameter storage is incomplete:** pulse-time SDO is unused, EEPROM validation is a stub, EEPROM parameters are not used by the force formula, and SDO parameter writes are not persisted.
6. **Scheduling validation:** compile/host-test results do not establish 1 ms deadlines, sensor rates, minimum stack margins or electrical bus recovery. See [RTOS runtime](RTOS.md) and [change log](../CHANGELOG.md).
7. **Watchdog limits:** the independent HAL timer checks for a control stall over 100 ms. It is not a hardware IWDG and cannot operate with IRQs globally disabled. A stalled individual worker is reported through freshness checks where applicable.
8. **Memory placement:** DMA buffers must remain in their accessible regions; SPI6 BDMA requires D3 SRAM. Startup zeroing and MPU windows are part of the runtime and must survive CubeMX regeneration.
9. **Board/device naming:** CubeMX uses `STM32H725VGH6`, while the VS Code debugger names `STM32H725RGVx`. Check the physical package and tooling configuration.
