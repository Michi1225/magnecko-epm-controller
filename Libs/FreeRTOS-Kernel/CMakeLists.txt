add_library(freertos_kernel STATIC
    tasks.c queue.c list.c event_groups.c
    portable/GCC/ARM_CM7/r0p1/port.c
)
target_include_directories(freertos_kernel PUBLIC
    include portable/GCC/ARM_CM7/r0p1 ${PROJECT_SOURCE_DIR}/Core/Inc
)
target_compile_definitions(freertos_kernel PRIVATE STM32H725xx)
