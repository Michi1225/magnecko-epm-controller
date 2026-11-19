cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME MagneckoFoot_Header)


# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C CXX ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

add_subdirectory(Libs/FreeRTOS-Kernel)
add_subdirectory(Libs/SOES_Wrapper)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Libs/Runtime/app_runtime.cpp
    Libs/Runtime/rtos_io.cpp
    Core/Src/stm32h7xx_hal_timebase_tim.c
    Libs/Sensors/BNO086.cpp
    Libs/Sensors/TMAG5273.cpp
    Libs/Sensors/TMF8829.cpp
    Libs/Sensors/LDC1101.cpp
    Libs/FSM/FSM.cpp
    Libs/Controller/FootController.cpp
    Libs/Controller/LEDController.cpp
    Libs/Controller/utils.cpp
    Libs/Controller/Charger.cpp
    Libs/Sensors/tmf8829_image.c
    Libs/Sensors/thermistor.c
    Libs/Controller/eeprom.cpp
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Libs/Sensors
    Libs/Runtime
    Libs/FSM
    Libs/Controller
    Drivers/VL53LMZ_ULD_API/Platform
    Drivers/VL53LMZ_ULD_API/inc
    Core/Inc
    # Add user defined include paths
)

# Add project symbols (macros)
option(EPM_USE_BATTERIES "Use batteries instead of the capacitor charger link" ON)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    EPM_USE_BATTERIES=$<BOOL:${EPM_USE_BATTERIES}>
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    freertos_kernel
    soes

    # Add user defined libraries
)

set(EPM_NUMBER 3 CACHE STRING "EPM hardware number")
set(VERSION_HEADER_DIR "${CMAKE_BINARY_DIR}/generated")
set(VERSION_HEADER "${VERSION_HEADER_DIR}/version.h")

file(MAKE_DIRECTORY "${VERSION_HEADER_DIR}")

add_custom_target(generate_version_header ALL
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DOUTPUT_FILE=${VERSION_HEADER}
        -DEPM_NUMBER=${EPM_NUMBER}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
    BYPRODUCTS "${VERSION_HEADER}"
    COMMENT "Generating version.h"
)

add_dependencies(${PROJECT_NAME} generate_version_header)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${VERSION_HEADER_DIR}"
)
