add_executable(Clock)

# pull in common dependencies
target_sources(Clock PRIVATE Clock.cpp)

# pull in common dependencies and additional spi hardware support
target_link_libraries(Clock PRIVATE
        pico_stdlib
        hardware_pio
        hardware_dma
        hardware_spi
        )

# enable usb output, disable uart output
pico_enable_stdio_usb(Clock 1)
pico_enable_stdio_uart(Clock 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(Clock)

# add url via pico_set_program_url
example_auto_set_url(Clock)
