# Hardware reference

[Home](Home.md) · [Architecture](Architecture.md) · [Configuration](Configuration.md)

This page records MCU signals and firmware assignments. It does not specify board connector positions or supply limits; obtain those from the board design before wiring hardware.

## Target and clocks

[`MagneckoFoot_Header.ioc`](../MagneckoFoot_Header.ioc) selects **STM32H725VGH6**, a Cortex-M7 target. [`SystemClock_Config()`](../Core/Src/main.cpp) uses the HSI/PLL configuration for nominal **550 MHz SYSCLK**, **275 MHz HCLK**, and APB dividers of two. The toolchain uses the Cortex-M7 hard-float ABI. The `.vscode` debugger device string differs from the CubeMX selection and must be checked against the board.

## Bus assignments

| Peripheral | Device / purpose | Firmware reference |
| --- | --- | --- |
| SPI1 | TMF8829 ToF | `TOF_SPI_HANDLE` in `TMF8829.h` |
| SPI2 | Four LDC1101 devices with separate chip selects | `LDC_SPI_HANDLE` in `LDC1101.h` |
| SPI3 | BNO086 IMU | `BNO086_SPI_HANDLE` in `BNO086.h` |
| SPI4 | External EtherCAT slave controller | `ECAT_SPI_HANDLE` in `FootController.h` |
| SPI6 | Capacitor charger link | `CHARGER_SPI_HANDLE` in `Charger.h` |
| I2C1 | LED controller, 7-bit address `0x2D` | `LEDController.h` |
| I2C2 | TMAG5273 Hall sensors, `0x35`, `0x22`, `0x78`, `0x44` | `TMAG5273.cpp` |
| I2C4 | Parameter EEPROM, address `0x50`; ID address constant `0x58` | `eeprom.h` |
| ADC1 | Temperature sensor sample via DMA | `thermistor.c` |
| CRC | Charger packet validation | `Charger.cpp` |

`MX_*` initialization in [`Core/Src`](../Core/Src) is the authoritative pin alternate-function and DMA configuration. I2C addresses above are unshifted; HAL calls shift them as needed.

## Control and diagnostic GPIO

| Signal | MCU pin | Implemented use |
| --- | --- | --- |
| `DRV1` | PA8 / TIM1 CH1 | Demagnetization pulse |
| `DRV2` | PC9 / TIM8 CH4 | Magnetization pulse |
| `DISCHARGE` | PA10 | Low requests capacitor discharge; high releases discharge |
| `GD_nEN` | PC8 | Low enables gate drivers |
| `GD_nFLT` | PC7 | Low indicates gate-driver fault while enabled |
| `MAG_STAT` | PB0 | Software magnetization flag output |
| `BUTTON` | PB2 | Active-low button with edge interrupt and TIM5 hold timer |
| `ECAT_NCS` | PE4 | EtherCAT SPI chip select |
| `nRST_ECAT` | PE1 | External EtherCAT reset |
| `EEPROM_LOADED` | PC14 | Startup waits for this input to become high |
| `IMU_INT` | PD1 | IMU message-ready indication |
| `TOF_INT` | PC5 | ToF data-ready path |

The full signal list is in [`main.h`](../Core/Inc/main.h). `ECAT_IRQ`, `SYNC0`, and `SYNC1` signals exist, but the application configures SOES for polling and has no distributed-clock validation handler.

## Timer roles

| Timer | Configuration / role |
| --- | --- |
| TIM1 | One-pulse output on CH1; nominal 1 µs ticks |
| TIM8 | Pulse output on CH4; manually enables channel and main output in `magnetize()` |
| TIM2 | RTOS control/acquisition release, nominal 1 ms |
| TIM4 | Configured at 2.5 ms; not started by RTOS runtime |
| TIM5 | Button hold, nominal 1 s |
| TIM6 | Pulse lockout, 10 µs ticks with `ARR = DEAD_TIME = 10000`, about 100 ms |
| TIM7 | Independent 1 ms HAL tick and control-progress monitor |
| TIM3 | Initialized by generated code; no application callback work assigned |

Pulse timers have a 20000-count period. `magnetize()` programs the appropriate compare value as `20000 - time`; the default pulse parameter is 5000 µs. Exact edge timing includes timer counting conventions and needs measurement if changing pulse behavior.

## Linker memory regions

[`STM32H725XG_FLASH.ld`](../STM32H725XG_FLASH.ld) declares:

| Region | Origin | Configured length |
| --- | --- | --- |
| FLASH | `0x08000000` | 1024 KiB |
| DTCMRAM | `0x20000000` | 128 KiB |
| RAM | `0x24000000` | 128 KiB |
| RAM_D2 | `0x30000000` | 32 KiB |
| RAM_D3 | `0x38000000` | 16 KiB |
| ITCMRAM | `0x00000000` | 64 KiB |

Ordinary `.data`, `.bss`, heap, and stack use DTCMRAM. Custom `.RAM` and `.RAM_D3` sections remain `NOLOAD`; startup zeros them before C++ constructors. SPI6 BDMA wire frames must remain in D3 SRAM. Both DMA windows have explicit noncacheable MPU regions. Static task stacks remain in DTCM; see [RTOS runtime](RTOS.md). Do not move DMA buffers into another memory region without reviewing the peripheral DMA path. These are linker allocations, not a complete statement of all physical device memory.
