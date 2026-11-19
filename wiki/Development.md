# Development workflow

[Home](Home.md) · [Architecture](Architecture.md) · [Troubleshooting](Troubleshooting.md)

## Where to make changes

- Add or change device behavior in [`Libs/Controller`](../Libs/Controller) and state transitions in [`Libs/FSM`](../Libs/FSM).
- Keep sensor protocols and conversion logic in [`Libs/Sensors`](../Libs/Sensors).
- Add source files to the top-level [`CMakeLists.txt`](../CMakeLists.txt); generated HAL sources are listed in [`cmake/stm32cubemx/CMakeLists.txt`](../cmake/stm32cubemx/CMakeLists.txt).
- Update `utypes.h`, the compiled C dictionary, and corresponding EtherCAT artifacts together for interface changes. Include the host-side process layout in the review.
- Treat `Drivers/` as vendored code and SOES as a pinned Git submodule. Avoid mistaking checked-in `CMakeFiles` or legacy generated Makefiles for project configuration.

## CubeMX regeneration

The entry point is **`Core/Src/main.cpp`**, although CubeMX normally generates `main.c`. Preserve custom code in `USER CODE` blocks and review generated diffs before building.

The repository provides [`cpp_to_c.bash`](../cpp_to_c.bash), which renames `main.cpp` to `main.c`, and [`c_to_cpp.bash`](../c_to_cpp.bash), which renames it back and replaces `main.c` references in the CubeMX CMake file. These scripts are simple destructive renames, not idempotent converters or backups. Use them only when the expected source filename exists and preserve a clean diff or backup first. After regeneration, inspect CMake sources, includes, callback bodies, GPIO assignments, clocks, DMA, interrupt priorities, and linker sections.

## Validation

The repository has no project-specific automated test suite. Vendored CMSIS tests are not controller tests. Choose checks based on the changed behavior:

1. Configure and build in a fresh directory with the intended ARM toolchain and explicit EPM number. The [currently observed GCC failure](Troubleshooting.md) must be resolved before claiming a successful firmware build.
2. Inspect the generated version header, linker map, RAM usage, and warnings. Verify DMA buffer placement if memory declarations changed.
3. For EtherCAT changes, compare mapping entries, byte totals (currently 4 output bytes and 264 input bytes), assignments, master configuration, and physical SII contents.
4. On a supported bench setup, check boot, required sensors, error codes, enable-state handshake, separate pulse directions, button hold, and lockout/retry behavior.
5. For fault or scheduling changes, measure relevant timings and verify bus loss, sensor failure, and fatal recovery behavior without relying on magnetic attachment to support a load.

Documentation-only changes can be checked without a board: verify Markdown links and consistency with source, and compile the LaTeX guide using its [build instructions](../user-guide/README.md). Keep physical test results separate from source-derived behavior.

## Updating this wiki

Update the source references and relevant pages whenever changing control words, status codes, PDO mappings, timing, compile-time flags, or calibration behavior. Keep the [standalone guide](../user-guide/user-guide.tex) synchronized with operator-facing changes. The baseline commit in [Home](Home.md) identifies the source snapshot used for the initial documentation; record a new baseline when revalidating substantive changes.
