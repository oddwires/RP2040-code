add_executable(pio_rotary_encoder)

# by default the header is generated into the build dir
pico_generate_pio_header(pio_rotary_encoder ${CMAKE_CURRENT_LIST_DIR}/pio_blink.pio)
pico_generate_pio_header(pio_rotary_encoder ${CMAKE_CURRENT_LIST_DIR}/pio_rotary_encoder.pio)
# however, alternatively you can choose to generate it somewhere else (in this case in the source tree for check in)

target_sources(pio_rotary_encoder PRIVATE pio_rotary_encoder.cpp)

target_link_libraries(pio_rotary_encoder PRIVATE
        pico_stdlib
        hardware_pio
        )

pico_add_extra_outputs(pio_rotary_encoder)

# add url via pico_set_program_url
example_auto_set_url(pio_rotary_encoder)

