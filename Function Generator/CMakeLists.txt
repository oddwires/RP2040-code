add_executable(FunctionGenerator)

pico_generate_pio_header(FunctionGenerator ${CMAKE_CURRENT_LIST_DIR}/rotary_encoder.pio)
pico_generate_pio_header(FunctionGenerator ${CMAKE_CURRENT_LIST_DIR}/blink.pio)
pico_generate_pio_header(FunctionGenerator ${CMAKE_CURRENT_LIST_DIR}/FastDAC.pio)
pico_generate_pio_header(FunctionGenerator ${CMAKE_CURRENT_LIST_DIR}/SlowDAC.pio)

target_sources(FunctionGenerator PRIVATE FunctionGenerator.cpp)

# pull in common dependencies and additional spi hardware support
target_link_libraries(FunctionGenerator PRIVATE
        pico_stdlib
        hardware_pio
        hardware_dma
        hardware_spi
        )

# create map/bin/hex file etc.
pico_add_extra_outputs(FunctionGenerator)

# add url via pico_set_program_url
example_auto_set_url(FunctionGenerator)

