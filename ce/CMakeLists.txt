# Mbed OS CE build of the NPR-70 firmware. The firmware sources stay unchanged (they also build on mbed-os 5.15.9);
# everything CE specific is in this directory. The sources are found in ../npr70/source (project repo) or ../source
# (GitHub repo, this directory is ce/), or at -DNPR_SRC_DIR=<path>. Build with build.sh.
cmake_minimum_required(VERSION 3.19)

set(NPR_FW_BUILD_ID "000000X" CACHE STRING "Firmware version string YYMMDD plus hour letter, set by build.sh")
set(NPR_MBED_CE_PATH "$ENV{NPR_CE_WORK}/mbed-os-7.1.0" CACHE PATH "Mbed OS CE checkout (tag mbed-os-7.1.0)")
option(NPR_HAL_TICK_WRAP "Wrap HAL_GetTick like the 5.15.9 build (hal_tick_wrap.cpp, mode 1)" ON)
option(NPR_UART_IRQ_PRIORITY "Put the console UART interrupt above all other interrupts" OFF)
option(NPR_DBG_NO_ECHO "Measurement only: console echo off" OFF)
option(NPR_DIAG "Console commands stack and latency, sections in debug (source/diag.cpp)" ON)
option(NPR_SCANF_FLOAT "Link float support into sscanf (set frequency, set freq_shift)" ON)

set(NPR_CONSOLE_BAUD 115200 CACHE STRING "Baud rate of the console UART (USB virtual COM port)")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mbed_app.json.in ${CMAKE_CURRENT_BINARY_DIR}/mbed_app.json @ONLY)
set(MBED_APP_JSON_PATH ${CMAKE_CURRENT_BINARY_DIR}/mbed_app.json CACHE INTERNAL "")
set(MBED_PATH ${NPR_MBED_CE_PATH} CACHE INTERNAL "")
set(MBED_CONFIG_PATH ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "")
include(${MBED_PATH}/tools/cmake/mbed_toolchain_setup.cmake)
project(npr70_ce LANGUAGES C CXX ASM)
include(${MBED_PATH}/tools/cmake/mbed_project_setup.cmake)

set(NPR_SRC_DIR "" CACHE PATH "Firmware source directory (default: ../npr70/source or ../source)")
if(NOT NPR_SRC_DIR)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../npr70/source/main.cpp)
        set(NPR_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../npr70/source)
    elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../source/main.cpp)
        set(NPR_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../source)
    else()
        message(FATAL_ERROR "firmware sources not found: set -DNPR_SRC_DIR=<path to source/>")
    endif()
endif()
get_filename_component(NPR_SRC_DIR "${NPR_SRC_DIR}" REALPATH)

# The compiler embeds absolute source paths (__FILE__ in asserts and error messages). Map them to fixed names so
# that the binary does not depend on where the sources, Mbed OS CE and the build directory are.
add_compile_options(
    -ffile-prefix-map=${NPR_MBED_CE_PATH}=/mbed-os
    -ffile-prefix-map=${NPR_SRC_DIR}=/npr70/source
    -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=/npr70/ce
    -ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=/build)

# custom target NPR_L432KC (custom_targets.json5) needs a CMake target before mbed-os is added
add_library(mbed-npr-l432kc INTERFACE)
target_link_libraries(mbed-npr-l432kc INTERFACE mbed-nucleo-l432kc)
add_subdirectory(${MBED_PATH} ./mbed-os)

file(GLOB NPR_SOURCES ${NPR_SRC_DIR}/*.cpp)
list(FILTER NPR_SOURCES EXCLUDE REGEX "Virt_Chan\\.cpp$")      # not built in the Makefile either
if(NOT NPR_HAL_TICK_WRAP)
    list(FILTER NPR_SOURCES EXCLUDE REGEX "hal_tick_wrap\\.cpp$")
endif()

add_executable(npr70_ce ${NPR_SOURCES})
target_include_directories(npr70_ce PRIVATE ${NPR_SRC_DIR} ${NPR_SRC_DIR}/SI4463 ${NPR_SRC_DIR}/drivers/radio/Si446x)
target_compile_definitions(npr70_ce PRIVATE
    NPR_SPI2_ALT0=1 NPR_SPI_PERIPHERAL=1
    SERIAL_TX=CONSOLE_TX SERIAL_RX=CONSOLE_RX
    FW_BUILD_ID="${NPR_FW_BUILD_ID}")
target_compile_options(npr70_ce PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/compat/npr_ce_compat.h"
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-overloaded-virtual)
if(NPR_HAL_TICK_WRAP)
    target_compile_definitions(npr70_ce PRIVATE NPR_HAL_TICK=1)
    target_link_options(npr70_ce PRIVATE -Wl,--wrap,HAL_GetTick)
endif()
if(NPR_UART_IRQ_PRIORITY)
    target_compile_definitions(npr70_ce PRIVATE NPR_UART_IRQ_PRIORITY=1)
endif()
if(NPR_DBG_NO_ECHO)
    target_compile_definitions(npr70_ce PRIVATE NPR_DBG_NO_ECHO=1)
endif()
if(NOT NPR_DIAG)
    target_compile_definitions(npr70_ce PRIVATE NPR_DIAG=0)
endif()
if(NPR_SCANF_FLOAT)
    target_link_options(npr70_ce PRIVATE -Wl,-u,_scanf_float)
endif()
target_link_libraries(npr70_ce PRIVATE mbed-os)
mbed_set_post_build(npr70_ce)
