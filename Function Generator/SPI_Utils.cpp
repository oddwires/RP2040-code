#include "SPI_Utils.h"

void SPI_Init(int _clk, int _tx) {
// Initialise the SPI bus hardawre...    
    spi_init(SPI_PORT, 500000);                             // Set SPI bus at 0.5MHz...
    gpio_set_dir(_clk, GPIO_OUT) ;                          // Initialise remaining SPI connections...
    gpio_set_dir(_tx, GPIO_OUT) ;
    gpio_set_function(_clk, GPIO_FUNC_SPI);
    gpio_set_function(_tx, GPIO_FUNC_SPI);
}

void cs_select(int _gpio) {
    asm volatile("nop \n nop \n nop");
    gpio_put(_gpio, 0);                                     // Active low
    asm volatile("nop \n nop \n nop");
}

void cs_deselect(int _gpio) {
    asm volatile("nop \n nop \n nop");
    gpio_put(_gpio, 1);                                     // Inactive high
    asm volatile("nop \n nop \n nop");
}

void SPI_Display_Write(int _data) {
    uint8_t buff[2];
    buff[0] = _data / 256;                                  // MSB data
    buff[1] = _data % 256;                                  // LSB data
    cs_select(Display_CS);
    spi_write_blocking(SPI_PORT, buff, 2);
    cs_deselect(Display_CS);
}

void MCP41020_Write (uint8_t _ctrl, uint8_t _data) {
// Parameters: _ctrl - channel to write to ( 0b01=A, 0b10=B, 0b11=Both )
//             _data - value to be written ( 0-255 )
    uint8_t buff[2];
    buff[0] = _ctrl | 0x10 ;                                // Set control bit to Write data
    buff[1] = _data * 2.55 ;                                // Scale data byte (100%->255)
    cs_select(Level_CS) ;                                   // Transmit data to Digi-Pot
    spi_write_blocking(SPI_PORT, buff, 2) ;
    cs_deselect(Level_CS) ;
}    
