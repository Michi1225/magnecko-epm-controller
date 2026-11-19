# Configuration

[Home](Home.md) · [Build and flash](Build-and-Flash.md) · [Operation](Operation.md)

## Build and source settings

| Setting | Location / default | Effect |
| --- | --- | --- |
| `EPM_NUMBER` | CMake cache, default `3` | Written into generated `version.h`, then copied to `Obj.EPM_Number` at startup |
| Build preset | `Debug`, `RelWithDebInfo`, `Release`, `MinSizeRel` | Separate output directories under `build/` |
| `EPM_USE_BATTERIES` | CMake cache, default `ON` | Defines battery mode; `OFF` enables charger initialization and service-task BDMA exchanges |
| `EEPROM_WRITE` | Not defined by the default build | When defined, writes `eeprom_img` to EEPROM during startup before reading parameters |
| `MAGNETIZATION_TIME` | `FootController.h`, `5000` | Pulse width parameter in nominal microseconds for both pulse directions |
| `DEAD_TIME` | `FootController.h`, `10000` | TIM6 auto-reload with 10 µs ticks, about 100 ms lockout |
| `FORCE_ESTIMATE_WINDOW_SIZE` | `FootController.h`, `20` | Maximum moving-average sample count |
| `N_PARAMETERS` | `FootController.h`, `10` | Number of floats read into private EEPROM parameter storage |
| Force-model constants | `TMAG5273.h` | Coefficients used by the active force calculation |
| `DEVICE_VERSION` | `TMAG5273.h`, `2` | Selects Hall conversion constant |
| Temperature sensor type | `FSM_bg()` in `FootController.cpp`, `PT1000` | Selects temperature lookup and disconnection condition |
| Software watchdog | `FootController.h`, `100` | SOES counter reload; runtime accounts for elapsed HAL milliseconds |
| RTOS configuration | `Core/Inc/FreeRTOSConfig.h` | Static allocation, 1 kHz tick, IRQ syscall ceiling 5 |

`EPM_NUMBER` and `EPM_USE_BATTERIES` are CMake cache options. To compile the
charger path, configure with `-DEPM_USE_BATTERIES=OFF`. `EEPROM_WRITE` remains a
compiler definition, not merely a CMake variable. See [RTOS runtime](RTOS.md)
for task stacks, timeout/freshness thresholds and diagnostics.

Temperature thresholds are currently hardcoded in `FSM_bg()`: warning at 65 °C, fault request at 80 °C, an intended clear branch at 50 °C, and PT1000 disconnected/out-of-range at 240 °C. See [Operation](Operation.md) for the actual branch behavior rather than assuming complete hysteresis.

## EPM identity and version

```sh
cmake --preset Debug -DEPM_NUMBER=2
cmake --build --preset Debug --parallel
```

The [version generation script](../cmake/GenerateVersion.cmake) writes:

- `GIT_COMMIT_HASH`: eight-character hash, or `unknown` if Git lookup fails.
- `GIT_DIRTY`: zero or one based on the working tree.
- `GIT_VERSION`: hash with an optional `-dirty` suffix.
- `EPM_NUMBER`: configured hardware identity.

The value initialized at SDO `0x2100:02` can be read by the host. Its RW storage is not a persistent hardware identification mechanism. The startup Git version is temporarily copied as bytes into the beginning of `Force_Estimate_Params`; these bytes are not meaningful calibration floats. Hardware/software revision strings at `0x1009`/`0x100A` remain separate defaults.

## EEPROM and calibration limitations

[`eeprom.h`](../Libs/Controller/eeprom.h) specifies 256 pages of 32 bytes, addressed through I2C4 at `0x50`. `read_force_estimation_params_from_eeprom()` reads ten floats beginning at page zero, offset zero. The read helpers do not return HAL status; validation currently uses unconditional `is_valid = 1`.

Those private ten floats are not connected to the current force formula. The 250-element EtherCAT `Force_Estimate_Params` array is separate storage. No object-download callback writes either array back to EEPROM or updates the model. Do not treat SDO parameter writes as a working persistent calibration workflow.

Enabling `EEPROM_WRITE` writes an embedded image during every boot of that firmware. Use that option only for deliberate provisioning after reviewing the image and board. The image is in [`eeprom_img.h`](../Libs/Controller/eeprom_img.h).

This I2C parameter EEPROM path is separate from the external EtherCAT controller's startup EEPROM/SII loading. The latter is indicated by `EEPROM_LOADED` and has no provisioning workflow in this application.

## Runtime object storage versus implemented configuration

| Object | Storage accepts writes | Implemented effect |
| --- | --- | --- |
| `0x6040` control word | Yes | Drives the application FSM |
| `0x7000` magnet command | Yes | Rising-edge pulse requests, normally acted on only while operation enabled |
| `0x6060` operation mode | Yes | No controller mode selection implemented |
| `0x2100:01` pulse time | Yes | Not read by pulse generation |
| `0x2100:02` EPM number | Yes | Changes exported RAM storage; startup sets the build value |
| `0x2101` force parameters | Yes | No active model update or EEPROM persistence |

After any configuration change, rebuild, check the generated identity, and verify the corresponding behavior on a supported bench setup. See [Development](Development.md) for a focused validation sequence.
