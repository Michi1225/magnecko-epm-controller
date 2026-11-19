# FreeRTOS kernel

Unmodified subset of FreeRTOS-Kernel **V11.2.0**, MIT licensed.

- Source: https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.2.0
- Archive: https://codeload.github.com/FreeRTOS/FreeRTOS-Kernel/tar.gz/refs/tags/V11.2.0
- Archive SHA-256: `e148d3a442ac7652c998e0a932c4cc2a3c6abff66a3a0a96a7678c0cd092d41c`
- Included: tasks, queues, lists, event groups, public headers, GCC Cortex-M7
  r0p1 port (also supports later M7 core revisions), license and release history.
- Excluded: examples, other ports, dynamic heap implementations, software timers,
  stream buffers and co-routines. These features are not used by this firmware.

`CMakeLists.txt` and this provenance file are project additions. The kernel is
vendored so firmware builds do not download dependencies. To upgrade, replace the
upstream files together and repeat the RTOS validation in `wiki/RTOS.md`.
