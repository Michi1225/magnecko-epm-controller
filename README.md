# Magnecko EPM controller

FreeRTOS-based STM32H725 firmware for a Magnecko electro-permanent magnet (EPM) foot. The controller accepts EtherCAT commands, generates magnetization and demagnetization pulses, and publishes force estimates, temperature, IMU, distance, inductive, and Hall sensor data.

- **[RTOS migration change log](CHANGELOG.md)** — implementation details, validation, and remaining hardware checks.
- **[Codebase wiki](wiki/Home.md)** — architecture, hardware, build and flash, EtherCAT interface, operation, configuration, and troubleshooting.
- **[LaTeX user guide](user-guide/user-guide.tex)** — a standalone guide for setup and operation.
- **[Build the guide](user-guide/README.md)** — PDF compilation instructions.

Start with [Build and flash](wiki/Build-and-Flash.md), then read [Operation](wiki/Operation.md). The [known limitations](wiki/Troubleshooting.md) include an EtherCAT description mismatch that must be resolved before commissioning a new master configuration.

The CMake target and CubeMX project retain the name `MagneckoFoot_Header`. Documentation describes the checked-in implementation; board supply ratings, connector pinouts, and mechanical load limits are not supplied by this repository.
