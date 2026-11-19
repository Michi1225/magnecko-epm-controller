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

For example, `0xFF05` combines IMU and ToF initialization failure; `0xFF0F` indicates all four sensor groups. The ordinary command callback cancels simultaneous rising requests before the operation action sees them, so `0x03` need not produce `0xFF18`.

## Symptoms and checks

| Symptom | What to inspect |
| --- | --- |
| CMake cannot find ARM compiler or Ninja | Put the toolchain binaries on `PATH`; select a CMake preset |
| Missing `SOES/soes/*.c` | Run `git submodule update --init --recursive`; check SSH access or local HTTPS override |
| GCC 14 build fails in `esc_address()` | Existing incompatible receive-buffer pointer; details below |
| Startup remains in configuration pattern | Inspect `EEPROM_LOADED` and external ESC reset/power; the readiness wait has no timeout |
| Master PDO lengths or entries are unexpected | Current XML is stale; compare compiled C dictionary and actual EEPROM/SII |
| PREOP → SAFEOP fails with DC enabled | No distributed-clock check handler is configured; use a polling-compatible configuration |
| Master is OP but pulse commands do nothing | Check masked application state, command rising edges, entry-time request clearing, lockout, and temperature flags |
| Repeated identical command is ignored | Send zero long enough to be consumed before raising the bit again |
| A request within 100 ms is lost | The lockout drops requests; retry with a fresh edge after more than 100 ms and any required recharge interval |
| Capacitor voltage is always zero | Default `USE_BATTERIES` leaves charger uninitialized; zero is not a verified measurement |
| Force estimate is zero | Inspect software magnet flag, Hall initialization/values, and three-of-four LDC slip votes |
| Force briefly persists after demagnetizing | The moving average retains old samples until they leave its 20-sample window |
| A sensor value stops changing | Sensor PDOs lack freshness/validity flags; inspect driver state, interrupts, and transfer completion |
| Red blinking and communication stopped | Fatal handler disabled interrupts and halted; correct the cause and reset |
| Fault reset has no effect | Fatal handler does not reach a serviceable generic FSM fault state |
| Button does not release magnet | Check hold duration, dead time, temperature flag, available pulse energy, and whether MCU is halted |

## Observed clean-build failure

On 29 October 2026, a clean Debug configure with CMake, Ninja, ARM GCC **14.3.1**, and the initialized SOES submodule succeeded. Building failed at [`esc_hw.c:55`](../Libs/SOES_Wrapper/hal/esc_hw.c):

```text
error: passing argument 3 of 'HAL_SPI_TransmitReceive'
from incompatible pointer type [-Wincompatible-pointer-types]
```

`HAL_SPI_TransmitReceive()` expects a byte receive buffer, while `esc_address()` passes `&ESCvar.ALevent`. This needs an explicit, reviewed treatment of the receive representation and byte order in the firmware. Documentation work leaves that existing source unchanged and does not claim compatibility with an untested older toolchain.

## Integration limitations to retain when modifying the code

1. **EtherCAT artifact mismatch:** XML and C dictionary disagree, and JSON identity metadata differs too. Reconcile artifacts and actual SII before commissioning.
2. **No application safe-output hook:** the SOES watchdog can move the bus to an error state without issuing a demagnetization/discharge command in the application. Quick-stop action is also empty.
3. **Fatal fault recovery:** `FSM_faultReactionActive()` calls a halting `Error_Handler()`. Reset, rather than control-word fault reset, is required on that path.
4. **Temperature behavior:** PT1000 out-of-range is warning-only; stale exported temperature is possible. A 65–80 °C branch clears the overtemperature flag, so complete 80/50 °C hysteresis is not implemented.
5. **Parameter storage is incomplete:** pulse-time SDO is unused, EEPROM validation is a stub, EEPROM parameters are not used by the force formula, and SDO parameter writes are not persisted.
6. **Mailbox read length:** `ESC_read()` uses a 128-byte termination array and indexes it using requested length. Mailboxes are configured for 512 bytes, and the bounds assertion is commented out. Large reads need review before use.
7. **Interrupt transfer lifetime:** `LDC1101::read_data()` passes a stack-local transmit frame to nonblocking `HAL_SPI_TransmitReceive_IT()`. Review buffer lifetime and transfer timing when diagnosing corrupt LDC readings.
8. **Initialization and timing:** custom `NOLOAD` storage, filter members without explicit initialization, shared interrupt state, busy waits, and dynamic allocation in the control ISR need hardware validation. The nominal loop rate alone does not prove execution deadlines.
9. **Board/device naming:** CubeMX uses `STM32H725VGH6`, while the VS Code debugger names `STM32H725RGVx`. Check the physical package and tooling configuration.

These are observations from source inspection, not changes made by the documentation. A successful future build would still leave the hardware and EtherCAT integration checks necessary.
