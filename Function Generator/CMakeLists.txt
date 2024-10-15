# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Function_Generator C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Function_Generator FunctionGenerator.cpp )

pico_set_program_name(Function_Generator "Function_Generator")
pico_set_program_version(Function_Generator "0.1")

# Generate PIO header
# NOTE: Ninja doesn't always work with relative paths, so best to replace with an absolute...
# pico_generate_pio_header(Function_Generator ${CMAKE_CURRENT_LIST_DIR}/DAC.pio)
pico_generate_pio_header(Function_Generator "C:/Github/Sandpit/RP2040-code/Function Generator/DAC.pio")

# pull in common dependencies...
target_sources(Function_Generator PRIVATE
                    DAC_Class.cpp
                    FunctionGenerator.cpp
                    ClockModule.cpp
                    Demo.cpp
                    SPI_Utils.cpp
)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Function_Generator 0)
pico_enable_stdio_usb(Function_Generator 1)

# Add the standard library to the build
target_link_libraries(Function_Generator
        pico_stdlib
        hardware_pio
        hardware_dma
        hardware_spi)

# Add the standard include files to the build
target_include_directories(Function_Generator PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(Function_Generator 
        hardware_spi
        hardware_dma
        hardware_pio
        )

pico_add_extra_outputs(Function_Generator)
