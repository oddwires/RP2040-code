#pragma once

#include <stdint.h>
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "GPIO+Definitions.h"

void SPI_Init(int _clk, int _tx);
void cs_select(int _gpio);
void cs_deselect(int _gpio);
void SPI_Display_Write(int _data);
void MCP41020_Write(uint8_t _ctrl, uint8_t _data);
