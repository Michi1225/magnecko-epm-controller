# Sensors and force estimation

[Home](Home.md) · [Hardware](Hardware.md) · [EtherCAT interface](EtherCAT-Interface.md)

## Acquisition and exported data

| Component | Acquisition | Export |
| --- | --- | --- |
| BNO086 | SPI3; data-ready wakes IMU task; complete packets are serialized | Raw signed 16-bit gyro XYZ, linear acceleration XYZ, quaternion RIJK |
| TMF8829 | SPI1; ToF task waits for interrupt-clear and ranging-read completion | 64 distances and 64 SNR values |
| Four LDC1101 | SPI2; acquisition task reads one device per completed iteration targeting 1 kHz | Four raw `l_data` and four `rp_data` values |
| Four TMAG5273 | I2C2; acquisition task reads 3,2,1,0 using DMA completion notifications | Raw signed XYZ registers and internal converted magnitudes |
| PT1000 temperature input | ADC1 calibration and continuous DMA sample with lookup table | Signed temperature multiplied by 100 |
| Charger | SPI6 BDMA transfers in services, only when initialized | Output voltage in units of 10 mV |

The background action copies complete queued sensor snapshots into `Obj`; it does not attach timestamps or per-sample validity bits to the PDOs. A 1 kHz PDO update does not imply fresh IMU, ToF, or Hall data at 1 kHz. Failed/stalled acquisition may leave old or zero values; internal freshness checks raise warning/SDO error `0xFF19`. See [RTOS runtime](RTOS.md) for age limits.

## IMU representation

[`BNO086::update_outputs()`](../Libs/Sensors/BNO086.cpp) copies report fields without conversion to floating-point SI units. The acceleration exposed by the controller is **linear acceleration**, not the separate `output_accel` array. Internally quaternion storage is I/J/K/R; the dictionary exports **R/I/J/K**.

Apply the report-specific fixed-point scaling specified for the configured sensor reports in the host. The firmware does not encode that scaling in its object dictionary, so this wiki does not invent numeric scale factors.

## ToF pixel order

For pixel index `i` from 0 through 63, the controller exports `pixel_data[i / 8][i % 8]`. Subindex `i + 1` addresses the same pixel in `0x60A0` (distance) and `0x60A1` (SNR). This specifies row-major storage, not the physical sensor's orientation on the foot.

The sensor frame stores distance as `uint16_t`, but the PDO dictionary exports signed 16-bit distances. The controller passes raw distance and SNR through; confirm distance units, invalid codes, and orientation for the actual sensor configuration before interpreting a display or applying thresholds.

## Hall values and force model

[`TMAG5273.cpp`](../Libs/Sensors/TMAG5273.cpp) combines six received bytes into signed X/Y/Z registers. For the configured `DEVICE_VERSION = 2`, conversion in the source is:

```text
axis = raw_axis / 65536 * 2 * MAG_SENSITIVITY
MAG_SENSITIVITY = 250
magnitude = sqrt(x*x + y*y + z*z)
```

The driver documents these converted values as mT. The PDOs contain raw registers. The header's sensitivity naming/comment and range configuration are not a substitute for calibration against the actual sensor variant.

The force model averages the four magnitudes and uses the constants in [`TMAG5273.h`](../Libs/Sensors/TMAG5273.h):

```text
B = (b0 + b1 + b2 + b3) / 4
g = (B / A_B)^INV_M_B - C_B
F = A_F / (g + C_F)^N_F

A_B = 67.3407354       INV_M_B = 1.8913935
C_B = 0.0360662672     A_F = 595.83695046
C_F = 0.90280164       N_F = 4.03057196
```

`FootController::force_estimation()` multiplies this estimate by the software magnetization flag. The background action stores it in a moving window of at most 20 samples, averages the window, and sets the published result to zero if at least three LDC channels currently indicate slip. Intended output units are newtons, as documented by the driver, but this is an empirical estimate requiring validation for each foot and surface.

The force average is not explicitly cleared on demagnetization, so a recent nonzero value may decay through the window after the software magnet flag becomes zero. The model does not consume the EEPROM parameter array or the EtherCAT calibration parameter storage.

## Slip detection

[`SlipDetector::update()`](../Libs/Sensors/LDC1101.cpp) updates two integer filters from each channel's RP reading:

```text
fast += (x - fast) >> 2
slow += (x - slow) >> 5
slip_detected = (fast - slow) > 200
```

Three or four positive channel votes suppress `Obj.Force_Estimate`. Detection does not command demagnetization or alter the FSM. `SOD_PERSISTENCE` is declared but unused; no persistence count is implemented. The separate `FootController::slip_detected` member is reset on magnetization but is not the source of the per-LDC votes.

The filter state is not reset explicitly when magnetizing. Its members are explicitly initialized; startup zeros the containing `NOLOAD` storage before construction.

## Charger behavior

[`Charger.cpp`](../Libs/Controller/Charger.cpp) exchanges packed enable/fault-clear commands and readiness, activity, OC/OV/watchdog flags, input/output voltages, current, and CRC. Services snapshots the desired command into a separate D3 wire buffer, waits for BDMA, and checks CRC before publishing status. A failed transfer/CRC marks the sample invalid and retains the last good status. `transmit_receive()` does nothing until initialization succeeds.

With default `EPM_USE_BATTERIES=ON`, `wait_ready()` is skipped, `initialized` stays false, and periodic charger communication is inactive. Capacitor voltage then remains at its initial zero rather than representing a measured empty capacitor.
