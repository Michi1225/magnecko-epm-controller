# Build and flash

[Home](Home.md) · [Configuration](Configuration.md) · [Troubleshooting](Troubleshooting.md)

## Requirements

- Git, including access to the SOES submodule.
- CMake 3.22 or newer and Ninja, as required by the checked-in build and presets.
- An ARM embedded GCC toolchain exposing `arm-none-eabi-gcc`, `arm-none-eabi-g++`, and associated binutils on `PATH`.
- STM32CubeProgrammer and an ST-Link/SWD connection for flashing.
- Optionally VS Code with CMake Tools, Cortex-Debug, and the STM32 tooling used by the checked-in editor configuration.

The toolchain file targets Cortex-M7 with `fpv5-d16` and the hard-float ABI. C sources use C11. C++ has no explicit language standard in CMake; the source uses C++17 constructs such as structured bindings, so the compiler default must support them. RTTI, exceptions, and thread-safe static initialization are disabled by the toolchain file.

## Initialize dependencies

From the repository root:

```sh
git submodule update --init --recursive
git submodule status
```

The submodule URL is SSH (`git@github.com:OpenEtherCATsociety/SOES.git`). If SSH access is unavailable, a local URL override avoids editing `.gitmodules`:

```sh
git config submodule.Libs/SOES_Wrapper/SOES.url https://github.com/OpenEtherCATsociety/SOES.git
git submodule update --init --recursive
```

Use the recorded submodule revision rather than updating to an arbitrary upstream branch.

## Configure and build

```sh
cmake --list-presets
cmake --preset Debug -DEPM_NUMBER=3
cmake --build --preset Debug --parallel
```

Choose `EPM_NUMBER` to match the foot identity expected by the host. The CMake default is **3**; the VS Code task offers **1–4** and defaults to **1**. Select the intended value explicitly when building a device image.

Other paired configure/build presets are `RelWithDebInfo`, `Release`, and `MinSizeRel`. For example:

```sh
cmake --preset Release -DEPM_NUMBER=3
cmake --build --preset Release --parallel
```

Expected Debug artifacts are `build/Debug/MagneckoFoot_Header.elf`, `build/Debug/MagneckoFoot_Header.map`, and `build/Debug/generated/version.h`. The version header is regenerated on each build and carries the eight-character Git hash, dirty flag/version, and EPM number. A checkout with uncommitted documentation also receives a `-dirty` version suffix.

The root `Makefile` is a separate legacy generated build; these instructions use the maintained CMake source list, SOES integration, and version-header generation.

### Observed build limitation

A clean Debug configure succeeded with ARM GCC **14.3.1** during documentation validation. Compilation then failed in [`esc_hw.c`](../Libs/SOES_Wrapper/hal/esc_hw.c) at the `HAL_SPI_TransmitReceive()` call in `esc_address()`: `&ESCvar.ALevent` is not the `uint8_t *` receive buffer type expected by HAL. This is an existing source issue, not a missing dependency. See [Troubleshooting](Troubleshooting.md). No successful firmware build or hardware test is claimed by this documentation.

## Flash a successfully built image

Confirm the physical board variant, EPM identity, and power arrangement before programming. The CubeMX project specifies `STM32H725VGH6`, while the editor launch configuration names `STM32H725RGVx`; reconcile that device setting with the actual board.

The following mirrors the repository's CubeProgrammer task:

```sh
STM32_Programmer_CLI --list
STM32_Programmer_CLI --connect port=swd --download build/Debug/MagneckoFoot_Header.elf -hardRst -rst --start
```

The ELF carries its load addresses. Optional raw images can be produced manually:

```sh
arm-none-eabi-objcopy -O ihex build/Debug/MagneckoFoot_Header.elf build/Debug/MagneckoFoot_Header.hex
arm-none-eabi-objcopy -O binary build/Debug/MagneckoFoot_Header.elf build/Debug/MagneckoFoot_Header.bin
```

These commands program the MCU; they do not provision the external EtherCAT controller's EEPROM/SII. That provisioning procedure and board wiring are not included in the repository.

## VS Code and debugging

[`.vscode/tasks.json`](../.vscode/tasks.json) provides `Build + Flash`, which selects an EPM number, configures, clean-builds, and downloads the selected target. Standalone build and launch paths may retain a previously cached EPM number.

[`.vscode/launch.json`](../.vscode/launch.json) includes ST-Link launch and attach configurations. Paths rely on the STM32 extension's CubeCLT setting. Launch runs to `main`; attach is useful for inspecting a running target. Use `controller.fsm_`, `controller.controller_error_word`, `controller.dead_time_active`, and `Obj` as initial watch expressions. Trace support is configured in [`utils.cpp`](../Libs/Controller/utils.cpp); there is no UART shell or built-in host CLI.
