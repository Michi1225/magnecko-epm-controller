# Operation

[Home](Home.md) · [EtherCAT interface](EtherCAT-Interface.md) · [Troubleshooting](Troubleshooting.md)

These steps describe the implemented command behavior. First complete [build and board setup](Build-and-Flash.md) and resolve the [ESI mismatch](EtherCAT-Interface.md). The firmware supplies no wiring diagram or validated supply ratings.

## Before enabling

Use the board's approved power and wiring arrangement. Support the mechanism independently before any release test: a demagnetization command or long button press can release the magnetic attachment. The firmware's magnet status is a software record of the last accepted pulse direction; it does not measure holding force or confirm a physical release. It initializes false after reset regardless of the magnet's physical state.

Bring the EtherCAT link through the master's normal state sequence to OP with the corrected mapping. Use a nominal 1 ms process cycle as a starting configuration consistent with the firmware polling rate. Keep all four output bytes initialized: control word zero, operation mode zero, and magnet command zero. Read `0x603F:00` for warnings and verify that the temperature and required sensors are plausible before pulse tests.

## Enable the application FSM

EtherCAT OP and application `OPERATION_ENABLED` are separate states. Exchange each control word until the masked status matches the expected value:

| Step | Control word | Expected `Status_Word & 0x007F` |
| --- | --- | --- |
| Initial disabled state | `0x0000` | `0x0050` — switch on disabled |
| Shutdown command | `0x0006` | `0x0031` — ready to switch on |
| Switch on | `0x0007` | `0x0033` — switched on |
| Enable operation | `0x000F` | `0x0037` — operation enabled |

The name “shutdown” is inherited from the drive FSM; here its state action enables charger command storage and keeps discharge asserted. The operation-enabled action releases discharge. State constants come from [`FSMTypes.hpp`](../Libs/FSM/FSMTypes.hpp) and differ from assumptions a generic drive host might make. Bit 7 (`0x0080`) indicates a warning and is outside the state mask.

Wait for operation enabled before raising a magnet command. Entry to that state clears pending requests, so a command sent during the transition may be discarded.

## Magnetize and demagnetize

Keep the control word at `0x000F` and operation mode at zero throughout a pulse request.

1. Send `Magnet_Command = 0x00` and allow at least one device processing cycle.
2. Send `0x01` to magnetize or `0x02` to demagnetize. Keep it present long enough for the 1 kHz firmware poll to see the edge.
3. Return the command to zero and let the device consume it before another request.
4. Check magnet status and the relevant sensor response. Allow **more than 100 ms** from the last accepted pulse before the next request, plus any board-specific recharge interval.

The default pulse is **5000 µs (5 ms)**. `magnetize()` accepts pulse parameters from 1000 through 10000 µs, rejects a pulse during dead time or an overtemperature flag, and updates magnet status when it starts the timer. Network pulse width is fixed by `MAGNETIZATION_TIME`; writing SDO `Pulse_Time` does not change it.

A rejected request is cleared by the operation-enabled action and is **not queued** for retry. A bit left high does not generate another edge. To retry, send zero, allow processing, and then raise the appropriate bit again after the lockout has elapsed. Never send both command bits together.

Conceptual process loop (adapt to the master's API):

```text
exchange(control=0x0000, mode=0, magnet=0); wait for state 0x0050
exchange(control=0x0006, mode=0, magnet=0); wait for state 0x0031
exchange(control=0x0007, mode=0, magnet=0); wait for state 0x0033
exchange(control=0x000F, mode=0, magnet=0); wait for state 0x0037
exchange(control=0x000F, mode=0, magnet=0x01)  # rising edge
exchange(control=0x000F, mode=0, magnet=0x00)  # rearm
wait beyond the pulse lockout and board recharge interval
exchange(control=0x000F, mode=0, magnet=0x02)  # release
exchange(control=0x000F, mode=0, magnet=0x00)
```

Each `exchange` represents continued cyclic service, not a one-shot API call. Add timeouts and handle bus/device faults in the host application; no master program is included here.

## Physical button

Pressing the active-low button starts TIM5; releasing it stops the timer. Holding it for about one second directly calls `magnetize()` with a demagnetization request. This path bypasses the application operation-enabled state check, but still respects dead time, pulse limits, and the overtemperature flag. A request blocked at that moment is not retried automatically; release and press again if appropriate. A halted controller cannot service this button.

## Disabling and shutdown

For a normal release, support the mechanism, demagnetize while operation is enabled, return the command to zero, and verify release independently. Then write control word `0x0000` and wait for `0x0050`; the disabled-state action disables the charger command and asserts discharge. Follow the board's physical discharge and power-removal procedure afterward. No discharge-complete measurement or timeout is implemented by this firmware.

Other implemented commands are `0x0007` to disable operation into switched on, `0x0006` to return toward ready, and `0x0002` for quick stop (`0x0017` masked status). The quick-stop state action is empty. Disabling operation, quick stop, loss of EtherCAT, and MCU reset do not themselves issue a demagnetization pulse. In particular, quick stop is not a physical release procedure.

## Temperature and faults

The active temperature model is PT1000. A valid reading of at least **65 °C** sets a warning; at least **80 °C** requests fault reaction. A PT1000 reading of at least **240 °C** is treated as disconnected/out of range and raises a warning instead of entering the overtemperature branch. That path may leave the exported temperature unchanged.

The source contains an intended clear branch at **50 °C**, but another branch clears the overtemperature flag for readings from 65 °C to below 80 °C. Do not assume a reliable 80/50 °C hysteresis. More immediately, the fault-reaction action calls `Error_Handler()`, which stops pulse outputs, asserts discharge, disables charger command storage, attempts charger communication, disables interrupts, and halts. Correct the condition and reset or power-cycle; do not rely on SDO/control-word recovery from that halt.

The generic FSM has `FAULT` (`0x0018`) and a fault-reset command (`0x0080`), but the controller's fatal path does not call `exitFaultReaction()` to reach a recoverable fault state. The final fault status/error may not be transmitted before communication stops.

## LED indications

| Indication | Where selected |
| --- | --- |
| Alternating blue/green configuration pattern | Beginning of controller initialization |
| Cyan | End of initialization and entry to switch-on-disabled |
| All channels on | Entry to switched-on |
| Solid green | Entry to operation-enabled |
| Fast red blink | Fatal `Error_Handler()` |

Other sensor-specific animations exist in [`LEDController.cpp`](../Libs/Controller/LEDController.cpp), but initialization warnings currently set error flags without selecting those animations. A green LED therefore does not certify that every sensor initialized successfully.
