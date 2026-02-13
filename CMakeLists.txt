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


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME fdc_h5_tof)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(HOME ${CMAKE_CURRENT_SOURCE_DIR})
set(APP ${HOME}/src)
# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(vendor/cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

file(GLOB_RECURSE VENDOR_SOURCES CONFIGURE_DEPENDS
    ${HOME}/usb/Class/CDC/Src/*.c
    ${HOME}/usb/Core/Src/*.c
    ${HOME}/vendor/Core/Src/mpu.c
    ${HOME}/driver/VL53L5CX_ULD_API/src/*.c
    ${HOME}/middleware/ai/*.c
    ${APP}/app/*.c
    ${APP}/bsp/*.c
    ${HOME}/library/nanopb/*.c
    ${HOME}/library/nanopb/generate/*.c
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${VENDOR_SOURCES}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
# Add user defined include paths
    ${HOME}/usb/Class/CDC/Inc
    ${HOME}/usb/Core/Inc
    ${HOME}/driver/VL53L5CX_ULD_API/inc
    ${HOME}/middleware/ai
    ${APP}/bsp
    ${APP}/app
    ${APP}/app/core
    ${APP}/app/logic
    ${HOME}/library/nanopb/generate
    ${HOME}/library/nanopb

)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
