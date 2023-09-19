// TBD: 1) SPI read connecton
//      2) Capacitors on op-amps
//      3) Issue with phase lock - red/writes to serial port affecting phase lock at high frequencies

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "blink.pio.h"
#include "DAC.pio.h"
#include "hardware/gpio.h"                          // Required for manually toggling GPIO pins (clock)

//////////////////////////////////////
// Define GPIO connections for Pico...
//////////////////////////////////////

// Note: The SPI Port only works through specific pins, so these connections are defined first.
// SPI Port connections...                          
#define SPI_PORT        spi0                        // Using SPI port 0
                                                    // ┌──────────┬───────────────┬─────────────┐────────────────┐
                                                    // │ PGA2040  │ Connection    │ MCP41010    │ Display module │
                                                    // ├──────────┼───────────────┼─────────────┤────────────────┤
#define PIN_RX          16                          // │ GPIO 16  │ SPI0 RX       │ (unused)    │  (unused)      │
#define Display_CS      17                          // │ GPIO 17  │ Chip select   │ (unused)    │  SS1 (white)   │
#define PIN_CLK         18                          // │ GPIO 18  │ SPI0 Clock    │ SCK (Pin 2) │  SCK (blue)    │
#define PIN_TX          19                          // │ GPIO 19  │ SPI0 TX       │ SI  (Pin 3) │  SDI (green)   │
#define Level_CS        20                          // │ GPIO 20  │ Chip select   │ CS  (Pin 1) │  (unused)      │
                                                    // └──────────┴───────────────┴─────────────┘────────────────┘

#define _DAC_A           0                          // DAC channel alias
#define _DAC_B           1                          // DAC channel alias
#define _Up              1
#define _Down           -1
#define _Sine_           0                          // Permited values for variable WaveForm_Type
#define _Square_         1
#define _Triangle_       2
#define _Time_           3
#define _Funct_          4
#define _Phase_          5
#define _Freq_           6
#define _Level_          7
#define _Duty_           8
#define _Range_          9
#define _Harmonic_      10
#define eof            255                          // EOF in stdio.h -is -1, but getchar returns int 255 to avoid blocking
#define CR              13
#define BitMapSize     256                          // Match X to Y resolution
#define MWidth          12                          // Width of terminal command margin (in columns)

//#define SysClock       125                        // System clock     for 0.488 MHz DAC output (Pico default)
//#define SysClock       250                        // System clock x 2 for 0.977 MHz DAC output
#define SysClock       280                          // Overclock        for 1.000 MHz DAC output

// Data for the clock face is generated externally using an Excel spreadsheet...
uint8_t FaceX[] = {
0xfa,0xfb,0xfc,0xfd,0xfe,0xff,0xff,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfd,0xfd,0xfd,0xfc,0xfc,0xfb,0xfb,0xfa,0xfa,0xf9,0xf8,0xf8,0xf7,0xf6,0xf5,0xf4,0xf3,0xf3,0xf2,0xf1,0xf0,0xef,0xe9,0xea,0xeb,0xec,0xed,0xed,0xec,0xeb,0xea,0xe9,0xe7,0xe6,0xe5,0xe3,0xe2,0xe1,0xdf,0xde,0xdc,0xdb,0xd9,0xd8,0xd6,0xd4,0xd3,0xd1,0xcf,0xcd,0xcc,0xca,0xc8,0xc6,0xc4,0xc3,0xc1,0xbc,0xbd,0xbd,0xbe,0xbe,0xbf,0xbd,0xbb,0xb9,0xb7,0xb5,0xb3,0xb1,0xaf,0xad,0xab,0xa9,0xa6,0xa4,0xa2,0xa0,0x9e,0x9c,0x9a,0x97,0x95,0x93,0x91,0x8f,0x8c,0x8a,0x88,0x86,0x83,0x81,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7d,0x7b,0x78,0x76,0x74,0x72,0x6f,0x6d,0x6b,0x69,0x67,0x64,0x62,0x60,0x5e,0x5c,0x5a,0x58,0x55,0x53,0x51,0x4f,0x4d,0x4b,0x49,0x47,0x45,0x43,0x41,0x42,0x41,0x41,0x40,0x40,0x3f,0x3d,0x3b,0x3a,0x38,0x36,0x34,0x32,0x31,0x2f,0x2d,0x2b,0x2a,0x28,0x26,0x25,0x23,0x22,0x20,0x1f,0x1d,0x1c,0x1b,0x19,0x18,0x17,0x15,0x14,0x13,0x12,0x15,0x14,0x13,0x12,0x11,0x11,0x0f,0x0e,0x0d,0x0c,0x0b,0x0b,0x0a,0x09,0x08,0x07,0x06,0x06,0x05,0x04,0x04,0x03,0x03,0x02,
0x02,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x04,0x03,0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x04,0x05,0x06,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0b,0x0c,0x0d,0x0e,0x0f,0x15,0x14,0x13,0x12,0x11,0x11,0x12,0x13,0x14,0x15,0x17,0x18,0x19,0x1b,0x1c,0x1d,0x1f,0x20,0x22,0x23,0x25,0x26,0x28,0x2a,0x2b,0x2d,0x2f,0x31,0x32,0x34,0x36,0x38,0x3a,0x3b,0x3d,0x42,0x41,0x41,0x40,0x40,0x3f,0x41,0x43,0x45,0x47,0x49,0x4b,0x4d,0x4f,0x51,0x53,0x55,0x58,0x5a,0x5c,0x5e,0x60,0x62,0x64,0x67,0x69,0x6b,0x6d,0x6f,0x72,0x74,0x76,0x78,0x7b,0x7d,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x81,0x83,0x86,0x88,0x8a,0x8c,0x8f,0x91,0x93,0x95,0x97,0x9a,0x9c,0x9e,0xa0,0xa2,0xa4,0xa6,0xa9,0xab,0xad,0xaf,0xb1,0xb3,0xb5,0xb7,0xb9,0xbb,0xbd,0xbc,0xbd,0xbd,0xbe,0xbe,0xbf,0xc1,0xc3,0xc4,0xc6,0xc8,0xca,0xcc,0xcd,0xcf,0xd1,0xd3,0xd4,0xd6,0xd8,0xd9,0xdb,0xdc,0xde,0xdf,0xe1,0xe2,0xe3,0xe5,0xe6,0xe7,0xe9,0xea,0xeb,0xec,0xe9,0xea,0xeb,0xec,0xed,0xed,0xef,0xf0,0xf1,0xf2,0xf3,0xf3,0xf4,0xf5,
0xf6,0xf7,0xf8,0xf8,0xf9,0xfa,0xfa,0xfb,0xfb,0xfc,0xfc,0xfd,0xfd,0xfd,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x4c,0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,0x1f,0x1e,0x1d,0x1c,0x1c,0x1c,0x1c,0x1c,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,0x1f,0x1e,0x1d,0x1c,0x18,0x17,0x16,0x15,0x14,0x13,0x12,0x11,0x10,0x0f,0x0e,0x0d,0x0c,0x0c,0x0c,0x0c,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x0e,0x0d,0x0c,0x0c,0x0c,0x0c,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,0x1f,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x26,0x25,0x24,0x23,0x22,0x21,0x20,0x1f,0x1e,0x1d,0x1c,0x1b,0x52,0x51,0x50,0x4f,0x4e,0x4d,0x4c,0x4b,0x4a,0x49,0x48,0x47,0x46,0x46,0x46,0x46,0x46,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x51,
0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x51,0x50,0x4f,0x4e,0x4d,0x4c,0x4b,0x4a,0x49,0x48,0x47,0x46,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x80,0x81,0x82,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x82,0x81,0x80,0x7f,0x7e,0x7d,0x7c,0x7b,0x7a,0x79,0x78,0x77,0x77,0x77,0x77,0x77,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x80,0x81,0x82,0xb6,0xb6,0xb6,0xb6,0xb6,0xb6,0xb6,0xb6,0xb6,0xb5,0xb4,0xb3,0xb2,0xb1,0xb0,0xaf,0xaf,0xaf,0xaf,0xaf,0xaf,0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xe0,0xe1,0xe2,0xe3,0xe3,0xe3,0xe3,0xe3,0xe3,0xe2,0xe1,0xe0,0xdf,0xde,0xdd,0xdc,0xdb,0xda,0xd9,0xd8,0xd7,0xd7,0xd7,0xd7,0xd7,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,0xe1,0xe2,0xe3,0xe3,0xe3,0xe3,0xe2,0xe1,0xe0,0xdf,0xde,0xdd,0xdc,0xdb,0xda,0xd9,0xd8,0xd7,0xd7,0xd7,0xd7,0xd8,0xd9,0xf0,0xef,0xee,0xed,0xec,0xeb,0xea,0xe9,0xe8,0xe7,0xe6,0xe5,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,
0xeb,0xec,0xed,0xee,0xef,0xf0,0xf0,0xf0,0xf0,0xf0,0xef,0xee,0xed,0xec,0xeb,0xea,0xe9,0xe8,0xe7,0xe6,0xe5,0xe4,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xe2,0xda,0xd9,0xd8,0xd7,0xd6,0xd5,0xd4,0xd3,0xd2,0xd1,0xd0,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xcf,0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xdb,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x81,0x80,0x7f,0x7e,0x7d,0x7c,0x7b,0x7a,0x79,0x78,0x77,0x76,0x75,0x75,0x75,0x75,0x75,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x80,0x81,0x80,0x7f,0x7e,0x7d,0x7c,0x7b,0x7a,0x79,0x78,0x77,0x76,0x75,} ;
uint8_t FaceY[] = {
0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7d,0x7b,0x78,0x76,0x74,0x72,0x6f,0x6d,0x6b,0x69,0x67,0x64,0x62,0x60,0x5e,0x5c,0x5a,0x58,0x55,0x53,0x51,0x4f,0x4d,0x4b,0x49,0x47,0x45,0x43,0x41,0x42,0x41,0x41,0x40,0x40,0x3f,0x3d,0x3b,0x3a,0x38,0x36,0x34,0x32,0x31,0x2f,0x2d,0x2b,0x2a,0x28,0x26,0x25,0x23,0x22,0x20,0x1f,0x1d,0x1c,0x1b,0x19,0x18,0x17,0x15,0x14,0x13,0x12,0x15,0x14,0x13,0x12,0x11,0x11,0x0f,0x0e,0x0d,0x0c,0x0b,0x0b,0x0a,0x09,0x08,0x07,0x06,0x06,0x05,0x04,0x04,0x03,0x03,0x02,0x02,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x04,0x03,0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x04,0x05,0x06,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0b,0x0c,0x0d,0x0e,0x0f,0x15,0x14,0x13,0x12,0x11,0x11,0x12,0x13,0x14,0x15,0x17,0x18,0x19,0x1b,0x1c,0x1d,0x1f,0x20,0x22,0x23,0x25,0x26,0x28,0x2a,0x2b,0x2d,0x2f,0x31,0x32,0x34,0x36,0x38,0x3a,0x3b,0x3d,0x42,0x41,0x41,0x40,0x40,0x3f,0x41,0x43,0x45,0x47,0x49,0x4b,0x4d,0x4f,0x51,0x53,0x55,0x58,0x5a,0x5c,0x5e,0x60,0x62,0x64,
0x67,0x69,0x6b,0x6d,0x6f,0x72,0x74,0x76,0x78,0x7b,0x7d,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x81,0x83,0x86,0x88,0x8a,0x8c,0x8f,0x91,0x93,0x95,0x97,0x9a,0x9c,0x9e,0xa0,0xa2,0xa4,0xa6,0xa9,0xab,0xad,0xaf,0xb1,0xb3,0xb5,0xb7,0xb9,0xbb,0xbd,0xbc,0xbd,0xbd,0xbe,0xbe,0xbf,0xc1,0xc3,0xc4,0xc6,0xc8,0xca,0xcc,0xcd,0xcf,0xd1,0xd3,0xd4,0xd6,0xd8,0xd9,0xdb,0xdc,0xde,0xdf,0xe1,0xe2,0xe3,0xe5,0xe6,0xe7,0xe9,0xea,0xeb,0xec,0xe9,0xea,0xeb,0xec,0xed,0xed,0xef,0xf0,0xf1,0xf2,0xf3,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf8,0xf9,0xfa,0xfa,0xfb,0xfb,0xfc,0xfc,0xfd,0xfd,0xfd,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfa,0xfb,0xfc,0xfd,0xfe,0xff,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfd,0xfd,0xfd,0xfc,0xfc,0xfb,0xfb,0xfa,0xfa,0xf9,0xf8,0xf8,0xf7,0xf6,0xf5,0xf4,0xf3,0xf3,0xf2,0xf1,0xf0,0xef,0xe9,0xea,0xeb,0xec,0xed,0xed,0xec,0xeb,0xea,0xe9,0xe7,0xe6,0xe5,0xe3,0xe2,0xe1,0xdf,0xde,0xdc,0xdb,0xd9,0xd8,0xd6,0xd4,0xd3,0xd1,0xcf,0xcd,0xcc,0xca,0xc8,0xc6,0xc4,0xc3,0xc1,0xbc,0xbd,0xbd,0xbe,0xbe,0xbf,0xbd,0xbb,0xb9,0xb7,0xb5,0xb3,0xb1,0xaf,
0xad,0xab,0xa9,0xa6,0xa4,0xa2,0xa0,0x9e,0x9c,0x9a,0x97,0x95,0x93,0x91,0x8f,0x8c,0x8a,0x88,0x86,0x83,0x81,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x3e,0x3d,0x3c,0x3b,0x3b,0x3b,0x3b,0x3b,0x3b,0x3b,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x78,0x77,0x76,0x75,0x75,0x75,0x75,0x75,0x75,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x89,0x89,0x89,0x89,0x89,0x89,0x88,0x87,0x86,0xb3,0xb2,0xb1,0xb0,0xaf,0xae,0xad,0xac,0xab,0xaa,0xab,0xac,0xad,0xae,0xaf,0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xb3,0xb3,0xb3,0xb3,0xb3,0xb3,0xb3,0xb3,0xb3,0xb3,0xb3,0xb3,0xe2,0xe3,0xe4,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe4,0xe3,0xe2,0xe1,0xe0,0xdf,0xde,0xdd,0xdc,0xdb,0xda,0xda,0xda,0xda,0xda,0xda,0xda,0xda,0xda,
0xda,0xd9,0xd8,0xd7,0xd6,0xd5,0xd4,0xd3,0xd2,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xe0,0xdf,0xde,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xdd,0xde,0xdf,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,0xf0,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf1,0xf0,0xef,0xee,0xed,0xec,0xeb,0xea,0xe9,0xe8,0xe7,0xe6,0xe6,0xe6,0xe6,0xe6,0xe6,0xe6,0xe7,0xe8,0xe5,0xe4,0xe3,0xe2,0xe1,0xe0,0xdf,0xde,0xdd,0xdc,0xdb,0xda,0xd9,0xd8,0xd7,0xd6,0xd5,0xd4,0xd3,0xd2,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xd1,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbe,0xbe,0xbe,0xbe,0xbe,0xbe,0xbd,0xbc,0xbb,0xba,0xb9,0xb8,0xb7,0xb6,0xb5,0xb4,0xb3,0xb3,0xb3,0xb3,0xb3,0xb3,0xb2,0xb1,0xb0,0xaf,0xae,0xad,0xac,0xab,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xab,0xac,0xad,0xae,0xaf,0xb0,0xb1,0xb2,0x87,0x88,0x89,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a,0x8a,0x89,0x88,0x87,0x86,0x85,0x84,0x83,0x82,0x81,0x80,0x7f,0x7e,0x7d,0x7c,0x7b,0x7a,0x79,0x78,0x77,0x76,0x76,0x76,0x76,
0x76,0x76,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x7f,0x7e,0x7d,0x3b,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x4d,0x4e,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x4f,0x4e,0x4d,0x4c,0x4b,0x4a,0x49,0x48,0x47,0x46,0x45,0x44,0x43,0x42,0x41,0x40,0x3f,0x3e,0x3d,0x3c,0x3b,0x3b,0x3b,0x3b,0x3b,0x3b,0x3b,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x0e,0x0d,0x0c,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,} ;
// (Number of pixels: 1000)

// Store clock hands co-ordinates...
uint8_t HandsX[192] = {} ;                          // Each hand requires 64 bytes - 3x64=192
uint8_t HandsY[192] = {} ;
int Hours=0, Mins=0, Secs=0, LEDCtr=0, Angle, StartX, StartY, Radius ;
float Radians ;

int tmp ;

char MarginFW[MWidth+1], MarginVW[MWidth+1] ;       // Fixed Width & Variable Width strings to create a fixed margin
unsigned short DAC_channel_mask = 0 ;               // Binary mask to simultaneously start all DMA channels
const uint32_t transfer_count = BitMapSize ;        // Number of DMA transfers per event
const float _2Pi = 6.283;                           // 2*Pi
int ParmCnt = 0, Parm[4], WaveForm_Type ;           // Storage for 4 command line parameters
int SelectedChan, c, i = 0, dirn = 1, result ;
int MarginCount = 0 ;                               // Manual count of characters written to terminal - required to maintain margins
float MaxDACfreq ;
char inStr[30], outStr[2500], ResultStr[3000], LastCmd[30] ;                // outStr large enough to contain the HelpText string

static void MCP41020_Write (uint8_t _ctrl, uint8_t _data) ;                 // Forward definitions
static void SPI_Display_Write(int _data) ;

class DAC {
public:
    PIO pio;                                                                // Class wide var to share value with setter function
    unsigned short DAC_data[BitMapSize] __attribute__ ((aligned(2048))) ;   // Align DAC data (2048d = 0800h)
    int Funct, Range, PIOnum ;
    int Level, Freq, Phase, DutyC, Harm, RiseT ;
    uint StateMachine, ctrl_chan, data_chan, GPIO, SM_WrapBot, SM_WrapTop ; // Variabes used by the getter function...
    char name ;                                                             // Name of this instance
    float DAC_div ;

    void StatusString () {
        // Report the status line for the current DAC object, aligned to current margin settings.
        char Str1[4], Str2[200], Margin[40] ;                               // !  Max line length = 100 chars !
        if (Range == 1)       strcpy(Str1," Hz") ;                          // Asign multiplier suffix
        if (Range == 1000)    strcpy(Str1,"KHz") ;
        if (Range == 1000000) strcpy(Str1,"MHz") ;
        tmp = strlen(inStr) ;                                               // Get number of command line characters
        // Handle the instances where the length of the command line exceeds the Margin width...
        if (MarginCount >= MWidth) {
            printf("\n") ;                                                  // Start a newline
            strcpy(inStr,"") ;                                              // Clear the string
            MarginCount = 0 ;                                               // Update the length variable
        }
        MarginVW[MWidth - MarginCount] = '\0' ;                             // Calculate padding required  for command characters and cursor
        if (MarginCount == 0) { strcpy(Margin,MarginFW) ; }                 // Fixed Width margin if no command characters
        else                  { strcpy(Margin,MarginVW) ; }                 // Varable Width margin if command characters are present
            switch ( Funct ) {                                              // Calculate status sting...
                case _Sine_:
                    sprintf(Str2,"%sChannel %c: Freq:%3d%s Phase:%03d Level:%03d Wave:Sine Harmonic:%d\n", Margin, name, Freq, Str1, Phase, Level, Harm) ;
                    break;
                case _Triangle_:
                    if ((RiseT == 0) || (RiseT == 100)) {
                        sprintf(Str2,"%sChannel %c: Freq:%3d%s Phase:%03d Level:%03d Wave:Sawtooth\n", Margin, name, Freq, Str1, Phase, Level) ;
                    } else {
                        sprintf(Str2,"%sChannel %c: Freq:%3d%s Phase:%03d Level:%03d Wave:Triangle Rise time:%d%%\n", Margin, name, Freq, Str1, Phase, Level, RiseT) ;
                    }
                    break;
                case _Square_:
                    sprintf(Str2,"%sChannel %c: Freq:%3d%s Phase:%03d Level:%03d Wave:Square Duty cycle:%d%%\n", Margin, name, Freq, Str1, Phase, Level, DutyC) ;
                    break ;
                case _Time_:
                    sprintf(Str2,"%sChannel %c: Freq:%3d%s Phase:%03d Level:%03d Time\n", Margin, name, Freq, Str1, Phase, Level) ;
            }
        strcat(ResultStr,Str2) ;
        inStr[0] = '\0' ;                                                   // Reset input string
        MarginCount = 0 ;
    }

    int Set(int _type, int _val) {
        switch (_type) {
            case _Freq_:
                Freq  = _val ;                              // Frequency (numeric)
                DACspeed(Freq * Range) ;                    // Update State machine run speed
                break ;
            case _Phase_:
                Phase  = _val ;                             // Phase shift (0->355 degrees)
                DataCalc() ;                                // Recalc Bitmap and apply new phase value
                break ;
            case _Level_:
                if (_val > 100) _val = 100 ;                // Limit max val to 100%
                Level = _val ;
                MCP41020_Write(SelectedChan, Level) ;       // Control byte for the MCP42010 just happens to be the same value as the SelectedChan variable
                StatusString () ;                           // Update the terminal session
                break ;
            case _Sine_:
                Funct = _Sine_ ;
                Harm = _val ;                               // Optional command line parameter (default to zero if not provided)
                DataCalc() ;
                break ;
            case _Square_:
                Funct = _Square_ ;
                DutyC = _val ;                              // Optional command line parameter (default to 50% if not provided)
                DataCalc() ;
                break ;
            case _Triangle_:
                Funct = _Triangle_ ;
                RiseT = _val ;                              // Optional command line parameter (default to 50% if not provided)
                DataCalc() ;
                break ;
            case _Time_:
                Funct = _Time_ ;
                DataCalc() ;
        }
        return (_val) ;
    }

    int Bump(int _type, int _dirn) {
    // _type = Frequency / Phase / Level, Duty, _dirn = Up / Down (_Up = 1, _Down = -1)
        int val = 0 ;
        if (_type == _Freq_) {
            if ((Freq*Range==0) && (_dirn==_Down)) {                // Attempt to bump below lower limit
                MarginVW[MWidth - MarginCount] = '\0' ;             // Calculate padding required  for command characters and cursor
                strcpy(ResultStr,MarginVW) ;
                strcat(ResultStr,"Error - Minimum Frequency\n") ;
            }
            // TBD - remove hardcoded Max frequency            
            else if ((Freq*Range==1000000) && (_dirn==_Up)) {       // Attempt to bump above upper limit
//          else if ((Freq*Range>=MaxDACfreq) && (_dirn==_Up)) {    // Attempt to bump above upper limit          
                MarginVW[MWidth - MarginCount] = '\0' ;             // Calculate padding required  for command characters and cursor
                strcpy(ResultStr,MarginVW) ;
                strcat(ResultStr,"Error - Maximum Frequency\n") ;
            }
            else {                                                  // Not at max or min value...
                Freq += _dirn ;                                     // ... bump
                if ((Freq == 1000) && (_dirn == _Up)) {             // Range transition point
                    Freq = 1 ;                                      // Reset count
                    if (Range == 1)         Range = 1000 ;          // either Hz=>KHz
                    else if (Range == 1000) Range = 1000000 ;       // or     KHz=>MHz
                }
                if ((Freq==0) && (Range!=1) && (_dirn==_Down)) {    // Range transition point
                    Freq = 999 ;                                    // Reset count
                    if (Range == 1000)    Range = 1 ;               // either KHz=>Hz
                    else if (Range == 1000000) Range = 1000 ;       // or     MHz=>KHz
                }
                val = Freq ;
                DACspeed(Freq * Range) ;  }
            }
        if (_type == _Phase_) {
            Phase += _dirn ;
            if (Phase == 360)  Phase = 0 ;              // Top Endwrap
            if (Phase  < 0  )  Phase = 359 ;            // Bottom Endwrap
            val = Phase ;
            DataCalc(); }                               // Update Bitmap data to include new DAC phase
        if (_type == _Level_) {
            Level += _dirn ;
            if (Level > 100) { Level = 0 ;   }          // Top endwrap
            if (Level < 0  ) { Level = 100 ; }          // Bottom endwrap
            val = Level ; 
            MCP41020_Write(SelectedChan, Level) ;       // Control byte for the MCP42010 just happens to be the same value as the SelectedChan variable
            StatusString () ; }                         // Update the terminal session
        if (_type == _Square_) {
            DutyC += _dirn ;
            if (DutyC > 100) { DutyC = 0 ;   }          // Top endwrap
            if (DutyC < 0  ) { DutyC = 100 ; }          // Bottom endwrap
            val = DutyC ;
            DataCalc(); }                               // Update Bitmap with new Duty Cycle value
        if (_type == _Triangle_) {
            RiseT += _dirn ;
            if (RiseT > 100) { RiseT = 0 ;   }          // Top endwrap
            if (RiseT < 0  ) { RiseT = 100 ; }          // Bottom endwrap
            val = RiseT ;
            DataCalc(); }                               // Update Bitmap with new Duty Cycle value
        if (_type == _Sine_) {
            Harm += _dirn ;
            if (Harm > 10) { Harm = 0 ;   }             // Top endwrap
            if (Harm < 0 ) { Harm = 9 ;   }             // Bottom endwrap
            val = Harm ;
            DataCalc(); }                               // Update Bitmap with new Sine harmonic value
        return (val) ;
    }

    void DACspeed (int _frequency) {
    // If DAC_div exceeds 2^16 (65,536), the registers wrap around, and the State Machine clock will be incorrect.
    // A slow version of the DAC State Machine is used for frequencies below 17Hz, allowing the value of DAC_div to
    // be kept within range.
        float DAC_freq = _frequency * BitMapSize;                               // Target frequency...
        DAC_div = 2 * (float)clock_get_hz(clk_sys) / DAC_freq;                  // ...calculate the PIO clock divider required for the given Target frequency
        float Fout = 2 * (float)clock_get_hz(clk_sys) / (BitMapSize * DAC_div); // Actual output frequency
         if (_frequency >= 34) {                                                // Fast DAC ( Frequency range from 34Hz to 999Khz )
            SM_WrapTop = SM_WrapBot ;                                           // SM program memory = 1 op-code
            pio_sm_set_wrap (pio, StateMachine, SM_WrapBot, SM_WrapTop) ;       // Fast loop (1 clock cycle)
            // If the previous frequency was < 33Hz, we will have just shrunk the assembler from 4 op-codes down to 1.
            // This leaves the State Machine program counter pointing outside of the new WRAP statement, which crashes the SM.
            // To avoid this, we need to also reset the State Machine program counter...
            pio->sm[StateMachine].instr = SM_WrapBot ;                          // Reset State Machine PC to start of code
            pio_sm_set_clkdiv(pio, StateMachine, DAC_div);                      // Set the State Machine clock 
        } else {                                                                // Slow DAC ( 1Hz=>33Hz )
            DAC_div = DAC_div / 64;                                             // Adjust DAC_div to keep within useable range
            DAC_freq = DAC_freq * 64;
            SM_WrapTop = SM_WrapBot + 3 ;                                       // SM program memory = 4 op-codes
            pio_sm_set_wrap (pio, StateMachine, SM_WrapBot, SM_WrapTop) ;       // slow loop (64 clock cycles)
            // If the previous frequency was >= 34Hz, we will have just expanded the assembler code from 1 op-code up to 4.
            // The State Machine program counter will still be pointing to an op-code within the new WRAP statement, so will not crash.
            pio_sm_set_clkdiv(pio, StateMachine, DAC_div);                      // Set the State Machine clock speed
        }
        StatusString () ;                                                       // Update the terminal session
    }

    void DataCalc () {
        int i, j, v_offset = BitMapSize/2 - 1;                                                        // Shift sine waves up above X axis
        int _phase;
        float a,b,x1,x2,g1,g2;

        // Scale the phase shift to match data size...    
        _phase = Phase * BitMapSize / 360 ;                                                           // Input  range: 0 -> 360 (degrees)
                                                                                                      // Output range: 0 -> 255 (bytes)
        switch (Funct) {
            case _Sine_:
                Harm = Harm % 10;                                                                     // Sine harmonics cycles after 7
                for (i=0; i<BitMapSize; i++) {
                // Add the phase offset and wrap data beyond buffer end back to the buffer start...
                    j = ( i + _phase ) % BitMapSize;                                                  // Horizontal index
                    a = v_offset * sin((float)_2Pi*i / (float)BitMapSize);                            // Fundamental frequency...
                    if (Harm >= 1) { a += v_offset/3  * sin((float)_2Pi*3*i  / (float)BitMapSize); }  // Add  3rd harmonic
                    if (Harm >= 2) { a += v_offset/5  * sin((float)_2Pi*5*i  / (float)BitMapSize); }  // Add  5th harmonic
                    if (Harm >= 3) { a += v_offset/7  * sin((float)_2Pi*7*i  / (float)BitMapSize); }  // Add  7th harmonic
                    if (Harm >= 4) { a += v_offset/9  * sin((float)_2Pi*9*i  / (float)BitMapSize); }  // Add  9th harmonic
                    if (Harm >= 5) { a += v_offset/11 * sin((float)_2Pi*11*i / (float)BitMapSize); }  // Add 11th harmonic
                    if (Harm >= 6) { a += v_offset/13 * sin((float)_2Pi*13*i / (float)BitMapSize); }  // Add 13th harmonic
                    if (Harm >= 7) { a += v_offset/15 * sin((float)_2Pi*15*i / (float)BitMapSize); }  // Add 15th harmonic
                    if (Harm >= 8) { a += v_offset/17 * sin((float)_2Pi*17*i / (float)BitMapSize); }  // Add 17th harmonic
                    if (Harm >= 9) { a += v_offset/19 * sin((float)_2Pi*19*i / (float)BitMapSize); }  // Add 19th harmonic
                    DAC_data[j] = (int)(a)+v_offset;                                                  // Sum all harmonics and add vertical offset
                }
                break;
            case _Square_: 
                b = DutyC * BitMapSize / 100;                                                         // Convert % to value
                for (i=0; i<BitMapSize; i++) {
                    j = ( i + _phase ) % BitMapSize;                                                  // Horizontal index
                    if (b <= i) { DAC_data[j] = 0;   }                                                // First section low
                    else        { DAC_data[j] = 255; }                                                // Second section high
                }
                break;
            case _Triangle_: 
                x1 = (RiseT * BitMapSize / 100) -1;                                                   // Number of data points to peak
                x2 = BitMapSize - x1;                                                                 // Number of data points after peak
                g1 = (BitMapSize - 1) / x1;                                                           // Rising gradient (Max val = BitMapSize -1)
                g2 = (BitMapSize - 1) / x2;                                                           // Falling gradient (Max val = BitMapSize -1)
                for (i=0; i<BitMapSize; i++) {
                    j = ( i + _phase ) % BitMapSize;                                                  // Horizontal index
                    if (i <= x1) { DAC_data[j] = i * g1; }                                            // Rising  section of waveform...
                    if (i > x1)  { DAC_data[j] = (BitMapSize - 1) - ((i - x1) * g2); }                // Falling section of waveform
                }
                break ;
        }
    StatusString () ;                                                                                  // Update the terminal session
    }

public:
    // Each DAC channel consists of...
    //    BitMap data => DMA => FIFO => State Machine => GPIO pins => R-2-R module
    // Note: The PIO clock dividers are 16-bit integer, 8-bit fractional, with first-order delta-sigma for the fractional divider.
    //       This means the clock divisor can vary between 1 and 65536, in increments of 1/256.
    //       If DAC_div exceeds 2^16 (65,536), the registers will wrap around, and the State Machine clock will be incorrect.
    //       For frequencies below 34Hz, an additional 63 op-code delay is inserted into the State Machine assembler code. This slows
    //       down the State Machine operation by a factor of 64, keeping the value of DAC_div within range.
    // Parameters...
    //       _name = Name of this DAC channel instance
    //       _pio = Required PIO channel
    //       _GPIO = Port connecting to the MSB of the R-2-R resistor network.
    // Constructor
    int DAC_chan(char _name, PIO _pio, uint _GPIO) {
        pio = _pio, GPIO = _GPIO, name = _name ;                                // Copy parameters to class vars
        PIOnum = pio_get_index(pio) ;                                           // Print friendly value
        Funct = _Sine_, Freq = 100, Level = 50 ;                                // Start-up default values...
        Range = 1, Harm = 0, DutyC = 50, RiseT = 50 ;
        name == 'A' ? Phase=0 : Phase=180 ;                                     // Set Phase difference between channels
        int _offset;
        StateMachine = pio_claim_unused_sm(_pio, true);                         // Find a free state machine on the specified PIO - error if there are none.
        ctrl_chan = dma_claim_unused_channel(true);                             // Find 2 x free DMA channels for the DAC (12 available)
        data_chan = dma_claim_unused_channel(true);

        // Configure the state machine to run the DAC program...
            _offset = pio_add_program(_pio, &pio_DAC_program);                  // Use helper function included in the .pio file.
            SM_WrapBot = _offset;
            pio_DAC_program_init(_pio, StateMachine, _offset, _GPIO);

        //  Setup the DAC control channel...
        //  The control channel transfers two words into the data channel's control registers, then halts. The write address wraps on a two-word
        //  (eight-byte) boundary, so that the control channel writes the same two registers when it is next triggered.
        dma_channel_config fc = dma_channel_get_default_config(ctrl_chan);      // default configs
        channel_config_set_transfer_data_size(&fc, DMA_SIZE_32);                // 32-bit txfers
        channel_config_set_read_increment(&fc, false);                          // no read incrementing
        channel_config_set_write_increment(&fc, false);                         // no write incrementing
        dma_channel_configure(
            ctrl_chan,
            &fc,
            &dma_hw->ch[data_chan].al1_transfer_count_trig,                     // txfer to transfer count trigger
            &transfer_count,
            1,
            false
        );
        //  Setup the DAC data channel...
        //  32 bit transfers. Read address increments after each transfer.
        fc = dma_channel_get_default_config(data_chan);
        channel_config_set_transfer_data_size(&fc, DMA_SIZE_32);                // 32-bit txfers
        channel_config_set_read_increment(&fc, true);                           // increment the read adddress
        channel_config_set_write_increment(&fc, false);                         // don't increment write address
        channel_config_set_dreq(&fc, pio_get_dreq(_pio, StateMachine, true));   // Transfer when PIO SM TX FIFO has space
        channel_config_set_chain_to(&fc, ctrl_chan);                            // chain to the controller DMA channel
        channel_config_set_ring(&fc, false, 9);                                 // 8 bit DAC 1<<9 byte boundary on read ptr. This is why we needed alignment!
        dma_channel_configure(
            data_chan,                                                          // Channel to be configured
            &fc,                                                                // The configuration we just created
            &_pio->txf[StateMachine],                                           // Write to FIFO
            DAC_data,                                                           // The initial read address (AT NATURAL ALIGNMENT POINT)
            BitMapSize,                                                         // Number of transfers; in this case each is 2 byte.
            false                                                               // Don't start immediately. All 4 control channels need to start simultaneously
                                                                                // to ensure the correct phase shift is applied.
        );
        DAC_channel_mask += (1u << ctrl_chan) ;                                 // Save details of DMA control channel to global variable. This facilitates
                                                                                // atomic restarts of both channels, and ensures phase lock between channels.
        DataCalc() ;                                                            // Populate bitmap data.
        DACspeed(Freq * Range) ;                                                // Initialise State MAchine clock speed.

        return(StateMachine);
    }
};

bool Repeating_Timer_Callback(struct repeating_timer *t) {
    // Routine called 5 times per second...
    int i, steps=64, MidX=128, MidY=128 ;
    // printf("%d\n",LEDCtr) ;                                                  // Debug
    LEDCtr --  ;
    if (LEDCtr>0) {                                     
        // LED off, and no change to the time for 4 out of 5 cycles...
        gpio_put(PICO_DEFAULT_LED_PIN, 0);              // LED is connected to PICO_DEFAULT_LED_PIN
    } else  {
        // Falls through here once per second. 
        LEDCtr = 5 ;
        gpio_put(PICO_DEFAULT_LED_PIN, 1);              // LED is connected to PICO_DEFAULT_LED_PIN

        // Bump the clock...
        if ((++Secs)>59) Secs=0 ;                                               // Always bump seconds
        if (Secs==0) { if ((++Mins)>59 ) Mins=0 ;  }                            // Bump minutes when seconds = 0
        if ((Mins==0) && (Secs==0)) { if ((++Hours)>24) Hours=0 ; }             // Bump hours when minutes and seconds = 0

        // Calculate seconds hand...
        i=0, Radius=127 ;                                                       // Radius=Length of seconds hand
        Angle=270-(Secs*6) ;                                                    // Angle in degrees, shifted 90 degree anti-clockwise
        Radians=Angle*3.14159/180 ;                                             // Angle in radians
        StartX=Radius*cos(Radians)+MidX ;
        StartY=Radius*sin(Radians)+MidY ;
        while(i<steps) { HandsX[i]=StartX+i*(MidX-StartX)/steps ;
                        HandsY[i]=StartY+i*(MidY-StartY)/steps ;
                        i++ ; }
        // Calculate minutes hand...
        i=0, Radius=95 ;                                                        // Radius=Length of minutes hand
        Angle=270-(Mins*6) ;                                                    // Angle in degrees, shifted 90 degree anti-clockwise
        Radians=Angle*3.14159/180 ;                                             // Angle in radians
        StartX=Radius*cos(Radians)+MidX ;
        StartY=Radius*sin(Radians)+MidY ;
        i=0 ;
        while(i<steps) { HandsX[i+steps]=StartX+i*(MidX-StartX)/steps ;
                        HandsY[i+steps]=StartY+i*(MidY-StartY)/steps ;
                        i++ ; }
        // Calculate hours hand...
        i=0, Radius=64 ;                                                        // Radius=Length of hours hand
        // Note: Hours hand progresses between hours in 5 partial increments, each increment measuring 12 minutes.
        //       Each 12 minute increment adds an additional 6 degrees of rotation to the hours hand.
        Angle=5*(270-(((Hours%12)*6)+(Mins/12)%5)) ;                            // Angle in degrees, shifted 90 degree anti-clockwise,
                                                                                //   and scaled by 5 to provide range 0=>12
        Radians=Angle*3.14159/180 ;                                             // Angle in radians
        StartX=Radius*cos(Radians)+MidX ;
        StartY=Radius*sin(Radians)+MidY ;
        while(i<steps) { HandsX[i+2*steps]=StartX+i*(MidX-StartX)/steps ;
                        HandsY[i+2*steps]=StartY+i*(MidY-StartY)/steps ;
                        i++ ; }

        //  printf("%s%d:%d:%d - %d\n",MarginFW,Hours,Mins,Secs,tmp) ;          // Debug
    }
    return true;
}

void VerText () {
    // Print version info aligned to current margin settings...
    tmp = strlen(inStr) ;                                               // Get number of command line characters
    if (tmp != 0) tmp ++ ;                                              // If there are characters, Bump to also allow for cursor
    MarginVW[MWidth - tmp] = '\0' ;                                     // Calculate padding required for command characters and cursor
    sprintf(ResultStr, "%s|---------------------|\n"
                       "%s| Function Generator  |\n"
                       "%s|    Version 1.0.0    |\n"
                       "%s| 19th September 2023 |\n"
                       "%s|---------------------|\n", 
                       MarginVW, MarginFW, MarginFW, MarginFW, MarginFW ) ;
}

void HlpText () {
    // Print Help text aligned to current margin settings...
    // Note: Following string requires '%%%%' to print '%'
    //       HelpText string is copied to outStr using sprintf - this reduces '%%%%' to '%%'
    //       outStr is sent to terminal using printf           - this reduces '%%' to '%'
    tmp = strlen(inStr) ;                                               // Get number of command line characters
    if (tmp != 0) tmp ++ ;                                              // If there are characters, Bump to also allow for cursor
    MarginVW[MWidth - tmp] = '\0' ;                                     // Calculate padding required for command characters and cursor
    sprintf(ResultStr, "%sHelp...\n"
                       "%s?            - Help\n"
                       "%sV            - Version\n"
                       "%sI            - Info\n"
                       "%sS            - Status\n"
                       "%s<A/B/C>si    - Sine wave (default = no harmonics)\n"
                       "%s<A/B/C>sin   - Sine wave +nth harmonic     ( 0->9 )\n"
                       "%s<A/B/C>si+   - Sine wave harmonic + 1\n"
                       "%s<A/B/C>si-   - Sine wave harmonic - 1\n"
                       "%s<A/B/C>sq    - Square wave (default = 50%%%% duty cycle)\n"
                       "%s<A/B/C>sqnnn - Square wave with nnn%%%% duty cycle\n"
                       "%s<A/B/C>sq+   - Square wave duty cycle + 1%%%%\n"
                       "%s<A/B/C>sq-   - Square wave duty cycle - 1%%%%\n"
                       "%s<A/B/C>tr    - Triangle wave\n"
                       "%s<A/B/C>trnnn - Triangle wave with nnn%%%% rise time\n"
                       "%s<A/B/C>tr+   - Triangle wave rise time + 1%%%%\n"
                       "%s<A/B/C>tr-   - Triangle wave rise time - 1%%%%\n"
                       "%s<A/B/C>sw    - Sweep frequency (Low, High, Speed, Pause)\n"
                       "%s<A/B/C>frnnn - Frequency = nnn            ( 0->999 )\n"
                       "%s<A/B/C>fr+   - Frequency + 1\n"
                       "%s<A/B/C>fr-   - Frequency - 1\n"
                       "%s<A/B/C>phnnn - Phase = nnn                ( 0->359 degrees )\n"
                       "%s<A/B/C>ph+   - Phase + 1\n"
                       "%s<A/B/C>ph-   - Phase - 1\n"
                       "%s<A/B/C>lennn - Level = nnn                ( 0->100%%%% )\n"
                       "%s<A/B/C>le+   - Level + 1\n"
                       "%s<A/B/C>le-   - Level - 1\n"
                       "%s<A/B/C>ti    - Time mode (display analog clock)\n"
                       "%swhere...\n"
                       "%s<A/B/C> = DAC channel A,B or C=both\n"
                       "%snnn     = Three digit numeric value\n",
                       MarginVW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW,
                       MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW,
                       MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW,
                       MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW, MarginFW,
                       MarginFW ) ;
}

void SysInfo (DAC DACobj[] ) {
    // Print System Info and resource allocation detils, aligned to current margin settings...
    // Note: 1) The following string requires '%%%%' to print '%' because...
    //            a) ResultStr is copied to outStr using sprintf - this reduces '%%%%' to '%%'
    //            b) outStr is sent to terminal using printf     - this reduces '%%' to '%'
    //       2) There seems to be some upper limit to sprintf, so the string is split into two smaller parts.
    tmp = strlen(inStr) ;                                               // Get number of command line characters
    if (tmp != 0) tmp ++ ;                                              // If there are characters, Bump to also allow for cursor
    MarginVW[MWidth - tmp] = '\0' ;                                     // Calculate padding required for command characters and cursor
    // First part of string...
    sprintf(ResultStr,"%s|----------------------------------------------------------|\n"
                      "%s| System Info...                                           |\n"
                      "%s|----------------------------------------------------------|\n"
                      "%s|   Target board:             Pico                         |\n"
                      "%s|   RP2040 clock frequency:   %7.3fMHz                   |\n"
                      "%s|   Max DAC frequency:      %7.3fMHz                     |\n"
                      "%s|----------------------------|-----------------------------|\n"
                      "%s| DAC Channel A              | DAC Channel B               |\n"
                      "%s|----------------------------|-----------------------------|\n"
                      "%s|   Level:           %3d%%%%    |   Level:            %3d%%%%    |\n"
                      "%s|   Frequency:       %3d     |   Frequency:        %3d     |\n"
                      "%s|   Multiplier:  %7d     |   Multiplier:   %7d     |\n"
                      "%s|   Phase:           %3d     |   Phase:            %3d     |\n"
                      "%s|   Duty cycle:      %3d%%%%    |   Duty cycle:       %3d%%%%    |\n"
                      "%s|   Sine harmonic:     %1d     |   Sine harmonic:      %1d     |\n"
                      "%s|   Triangle Rise:   %3d%%%%    |   Triangle Rise:    %3d%%%%    |\n",
            MarginVW,     MarginFW,           MarginFW,           MarginFW, 
            MarginFW,   (float)clock_get_hz(clk_sys)/1000000,              
            MarginFW,       MaxDACfreq/1000000,
            MarginFW,       MarginFW,           MarginFW,
            MarginFW,       DACobj[_DAC_A].Level,               DACobj[_DAC_B].Level,
            MarginFW,       DACobj[_DAC_A].Freq,                DACobj[_DAC_B].Freq,
            MarginFW,       DACobj[_DAC_A].Range,               DACobj[_DAC_B].Range,
            MarginFW,       DACobj[_DAC_A].Phase,               DACobj[_DAC_B].Phase,
            MarginFW,       DACobj[_DAC_A].DutyC,               DACobj[_DAC_B].DutyC,
            MarginFW,       DACobj[_DAC_A].Harm,                DACobj[_DAC_B].Harm,
            MarginFW,       DACobj[_DAC_A].RiseT,               DACobj[_DAC_B].RiseT
            ) ;
    printf(ResultStr) ;                                         // Print first part of string
    // Second part of string...
    sprintf(ResultStr,"%s|----------------------------|-----------------------------|\n"
                      "%s|   Divider:      %10.3f |   Divider:       %10.3f |\n"
                      "%s|----------------------------|-----------------------------|\n"
                      "%s|   PIO:               %d     |   PIO:                %d     |\n"
                      "%s|   State machine:     %d     |   State machine:      %d     |\n"
                      "%s|   GPIO:           %d->%d     |   GPIO:           %d->%d     |\n"
                      "%s|  *BM size:    %8d     |  *BM size:     %8d     |\n"
                      "%s|  *BM start:   %x     |  *BM start:    %x     |\n"
                      "%s|   Wrap Bottom:      %2x     |   Wrap Bottom:       %2x     |\n"
                      "%s|   Wrap Top:         %2x     |   Wrap Top:          %2x     |\n"
                      "%s|   DMA ctrl:         %2d     |   DMA ctrl:          %2d     |\n"
                      "%s|   DMA data:         %2d     |   DMA data:          %2d     |\n"
                      "%s|----------------------------|-----------------------------|\n"
                      "%s  *BM = Bit map\n",
            MarginFW,
            MarginFW,       DACobj[_DAC_A].DAC_div,             DACobj[_DAC_B].DAC_div,
            MarginFW,
            MarginFW,       DACobj[_DAC_A].PIOnum,              DACobj[_DAC_B].PIOnum,
            MarginFW,       DACobj[_DAC_A].StateMachine,        DACobj[_DAC_B].StateMachine,
            MarginFW,       DACobj[_DAC_A].GPIO, DACobj[_DAC_A].GPIO+7,
                            DACobj[_DAC_B].GPIO, DACobj[_DAC_B].GPIO+7,
            MarginFW,       BitMapSize,                         BitMapSize,
            MarginFW, (int)&DACobj[_DAC_A].DAC_data[0],         
                      (int)&DACobj[_DAC_B].DAC_data[0],
            MarginFW,       DACobj[_DAC_A].SM_WrapBot,          DACobj[_DAC_B].SM_WrapBot,
            MarginFW,       DACobj[_DAC_A].SM_WrapTop,          DACobj[_DAC_B].SM_WrapTop, 
            MarginFW,       DACobj[_DAC_A].ctrl_chan,           DACobj[_DAC_B].ctrl_chan,
            MarginFW,       DACobj[_DAC_A].data_chan,           DACobj[_DAC_B].data_chan,
            MarginFW,       MarginFW
            ) ;             // Printing the final part of the string is handled by the calling routine.
                            // This prevents the 'unknown command' mechanism from triggering.
}

static inline void cs_select(int _gpio) {
    asm volatile("nop \n nop \n nop");
    gpio_put(_gpio, 0);                                                      // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(int _gpio) {
    asm volatile("nop \n nop \n nop");
    gpio_put(_gpio, 1);
    asm volatile("nop \n nop \n nop");
}

static void SPI_Display_Write(int _data) {
    uint8_t buff[2];
    buff[0] = _data / 256;                                                      // MSB data
    buff[1] = _data % 256;                                                      // LSB data
    cs_select(Display_CS);
    spi_write_blocking(SPI_PORT, buff, 2);
    cs_deselect(Display_CS);
}

static void MCP41020_Write (uint8_t _ctrl, uint8_t _data) {
    //   Add a control bit to select a 'Digi-Pot Write' command.
    //   Scale the data byte to be in the range 0->255.
    //   Transmit data over the SPI bus to the Digi-Pot.
    uint8_t buff[2];
    buff[0] = _ctrl | 0x10 ;                                // Set command bit to Write data
    buff[1] = _data * 2.55 ;                                // Scale data byte (100%->255)
    cs_select(Level_CS) ;                                   // Transmit data to Digi-Pot
    spi_write_blocking(SPI_PORT, buff, 2) ;
    cs_deselect(Level_CS) ;
}    

static void getLine() {
    char *pPos = (char *)inStr ;                            // Pointer to start of Global input string
    int count = 0 ;
    while(1) {
        c = getchar();
        if (c == eof || c == '\n' || c == '\r') break ;     // Non blocking exit
        putchar(c);                                         // FullDuplex echo
        *pPos++ = c ;                                       // Bump pointer, store character
        count ++ ;
    }
    *pPos = '\0' ;
    MarginCount += count ;                                  // Track number of characters on current line
    return ;
}

int SetVal(DAC DACobj[], int _Parm) {
    // Common code for setting frequency, duty cycle, phase, waveform and level.
    // Handles options to set a specific value or bump up/down...
    if (inStr[3] == '+') {                                              // Bump up and grab result for SPI display...
        if (SelectedChan & 0b01) result = DACobj[_DAC_A].Bump(_Parm,_Up);
        if (SelectedChan & 0b10) result = DACobj[_DAC_B].Bump(_Parm,_Up) ;
    } else if (inStr[3] == '-') {                                       // Bump down and grab result for SPI display...
        if (SelectedChan & 0b01) result = DACobj[_DAC_A].Bump(_Parm,_Down) ;
        if (SelectedChan & 0b10) result = DACobj[_DAC_B].Bump(_Parm,_Down) ;
    } else {                                                            // Not a bump, so set the absolute value from Parm[0]...
        if (SelectedChan & 0b01) result = DACobj[_DAC_A].Set(_Parm,Parm[0]) ;
        if (SelectedChan & 0b10) result = DACobj[_DAC_B].Set(_Parm,Parm[0]) ;
    }
    // Disable the Ctrl channels...
    hw_clear_bits(&dma_hw->ch[DACobj[_DAC_A].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    hw_clear_bits(&dma_hw->ch[DACobj[_DAC_B].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    // wait for Busy flag to clear...

    // Abort the data channels...
    dma_channel_abort(DACobj[_DAC_A].data_chan);
    dma_channel_abort(DACobj[_DAC_B].data_chan);

    // Reset the data transfer DMA's to the start of the data Bitmap...
    dma_channel_set_read_addr(DACobj[_DAC_A].data_chan, &DACobj[_DAC_A].DAC_data[0], false);
    dma_channel_set_read_addr(DACobj[_DAC_B].data_chan, &DACobj[_DAC_B].DAC_data[0], false);

    // Re-enable the Ctrl channels (doesn't restart data transfer)...
    hw_set_bits(&dma_hw->ch[DACobj[_DAC_A].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    hw_set_bits(&dma_hw->ch[DACobj[_DAC_B].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);

    dma_start_channel_mask(DAC_channel_mask);                           // Atomic restart both DAC channels
    return result ;
}

 int main() {
    bool InvX=false, InvY=false ;                                       // Clock display mode flags to allow inverted output
    set_sys_clock_khz(SysClock*1000, true) ;                            // Set Pico clock speed
    MaxDACfreq = clock_get_hz(clk_sys) / BitMapSize ;                   // Calculate Maximum DAC output frequency for given CPU clock speed
    stdio_init_all() ;

    spi_init(SPI_PORT, 500000);                                         // Set SPI0 at 0.5MHz...
    gpio_set_function(PIN_CLK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX, GPIO_FUNC_SPI);

    gpio_init(Display_CS) ;                                             // Initailse the required GPIO ports...
    gpio_set_dir(Display_CS, GPIO_OUT) ;
    gpio_put(Display_CS, 1) ;                                           // Chip select is active-low, so initialise to high state
    gpio_init(Level_CS) ;
    gpio_set_dir(Level_CS, GPIO_OUT) ;
    gpio_put(Level_CS, 1) ;                                             // Chip select is active-low, so initialise to high state
    gpio_init(PICO_DEFAULT_LED_PIN) ;
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT) ;
    gpio_set_dir(PIN_CLK, GPIO_OUT) ;                                   // Initialise remaining SPI connections...
    gpio_set_dir(PIN_TX, GPIO_OUT) ;

    for (int i=0; i<16; i++) {
        gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);                     // Setting Max slew rate and gpio drive strength keeps output 
        gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_12MA);           //   linear at high frequencies...
    }

    memset(MarginFW,' ',MWidth) ;                                       // Initialise Fixed Width margin...
    MarginFW[MWidth] = '\0' ;                                           //  ... and terminate
    memset(MarginVW,' ',MWidth) ;                                       // Initialise Variable Width margin...
    MarginVW[MWidth] = '\0' ;                                           //  ... and terminate
    ResultStr[0] = '\0' ;                                               // Reset string

    // Instantiate objects to control the various State Machines...
    // Note: Both DAC channels need to be on the same PIO to achieve
    //       Atomic restarts for accurate phase sync.
    DAC DACobj[2];                                                      // Array to hold the two DAC channel objects
    DACobj[_DAC_A].DAC_chan('A',pio1,0);                                // First  DAC channel object in array - resistor network connected to GPIO0->8
    DACobj[_DAC_B].DAC_chan('B',pio1,8);                                // Second DAC channel object in array - resistor network connected to GPIO8->16

    strcpy(LastCmd,"?") ;                                               // Hitting return will give 'Help'

    SPI_Display_Write(SysClock) ;                                       // Pico system clock speed (in MHz)
    MCP41020_Write(0x3, 50) ;                                           // Both channels -> 50% output level

    while (!stdio_usb_connected()) { sleep_ms(100); }                   // Wait for USB connection...

    SPI_Display_Write(DACobj[_DAC_A].Freq) ;                            // Frequency => SPI display

    // Send (optional) start-up messages to terminal...
    VerText() ;                                                         // Version text
    printf(ResultStr) ;                                                 // Update terminal

    // Atomic Restart - starting all 4 DMA channels simultaneously ensures phase sync between both DAC channels
    dma_start_channel_mask(DAC_channel_mask);           // Sets the 'Busy' flag in Ctrl reg

    struct repeating_timer timer;
    add_repeating_timer_ms(-200, Repeating_Timer_Callback, NULL, &timer) ;              // 5 x per second to blink LED

    while(1) {
        ParmCnt=0, Parm[0]=0,  Parm[1]=0,  Parm[2]=0,  Parm[3]=0 ;                      // Reset all command line parameters
        memset(MarginVW,' ',MWidth) ;                                                   // Re-initialise Variable Width margin...
        MarginVW[MWidth] = '\0' ;                                                       //  ... and terminate
        ResultStr[0] = '\0' ;                                                           // Reset string
        printf(">") ;                                                                   // Command prompt
        MarginCount = 1 ;                                                               // Reset count and bump for command prompt

        getLine() ;                                                                     // Fetch command line

        // Zero length string = 'CR' pressed...
        if (strlen(inStr) == 0) { strcpy(inStr,LastCmd) ;                               // Repeat last command    
                                  printf("%s", inStr) ; }

        // One character commands...
        if (strlen(inStr) == 1) {
            if (inStr[0] == '?') HlpText() ;                                            // Help text
            if (inStr[0] == 'V') VerText() ;                                            // Version text
            if (inStr[0] == 'S') { 
                DACobj[_DAC_A].StatusString() ; 
                DACobj[_DAC_B].StatusString() ; 
            }
            if (inStr[0] == 'I') SysInfo(DACobj);                                       // TBD - inconsitant - make these global ??
        }

        // For all remaining commands, the first character selects DAC channel A or B...
        if (inStr[0] == 'A') { SelectedChan = 0b0001; }                                 // Channel A only
        if (inStr[0] == 'B') { SelectedChan = 0b0010; }                                 // Channel B only
        if (inStr[0] == 'C') { SelectedChan = 0b0011; }                                 // Channel A & B

        // ...and if we aren't bumping a value, there will be one or more numeric parameters...
        if ((strlen(inStr) != 0) && (inStr[2] != '+') && (inStr[2] != '-')) {
            i = 2 ;                                                                     // Skip chars 0, 1 and 2
            while (i++ < strlen(inStr) ) {                                              // Starts at char 3
                if ( inStr[i] == 'H' ) {                                                // Hz suffix
                    if (SelectedChan & 0b01) DACobj[_DAC_A].Range = 1 ;
                    if (SelectedChan & 0b10) DACobj[_DAC_B].Range = 1 ;
                }
                else if ( inStr[i] == 'K' ) {                                           // KHz suffix
                    if (SelectedChan & 0b01) DACobj[_DAC_A].Range = 1000 ;
                    if (SelectedChan & 0b10) DACobj[_DAC_B].Range = 1000 ;
                    // If the command suffix has been entered as 'KHz', then the next loop itteration will detect the 'H' and
                    // overwrite the 'K'. Writing a space to the next char prevents, this from happening.
                    if (inStr[i+1] == 'H') inStr[i+1] = ' ' ;                           // Overwrite next char with space
                }
                else if ( inStr[i] == 'M' ) {                                           // MHz suffix
                    if (SelectedChan & 0b01) DACobj[_DAC_A].Range = 1000000 ;
                    if (SelectedChan & 0b10) DACobj[_DAC_B].Range = 1000000 ;
                    // If the command suffix has been entered as 'MHz', then the next loop itteration will detect the 'H' and
                    // overwrite the 'M'. Writing a space to the next char prevents, this from happening.
                    if (inStr[i+1] == 'H') inStr[i+1] = ' ' ;                           // Overwrite next char with space
                }
                else if ( inStr[i] == ',' )  { ParmCnt++ ; }                            // Next parameter
                else if (isdigit(inStr[i])) { Parm[ParmCnt] *= 10;                      // Next digit. Bump the existing decimal digits
                                              Parm[ParmCnt] += inStr[i] - '0'; }        // Convert character to integer and add
            }
        }

        // Next two chars select the command...
        if ((inStr[1]=='p')&(inStr[2]=='h')) SetVal(DACobj,_Phase_) ;            // Phase
        if ((inStr[1]=='l')&(inStr[2]=='e')) SetVal(DACobj,_Level_) ;            // Level
        if ((inStr[1]=='s')&(inStr[2]=='i')) SetVal(DACobj,_Sine_) ;             // Sine wave     (optional harmonic parameter)
        if ((inStr[1]=='f')&(inStr[2]=='r')) SetVal(DACobj,_Freq_) ;             // Frequency

        // The next two commands need different default values...
        if (strlen(inStr)==3) Parm[0] = 50 ;                                    // If no value provided, set default to 50
        if ((inStr[1]=='s')&(inStr[2]=='q')) SetVal(DACobj,_Square_) ;          // Set Square wave   (optional duty cycle parameter)
        if ((inStr[1]=='t')&(inStr[2]=='r')) SetVal(DACobj,_Triangle_) ;        // Set Triangle wave (optional duty cycle parameter)

        if ((inStr[1]=='t')&(inStr[2]=='i')) {                                  // Time display...
            // Disable the Ctrl channels...
            hw_clear_bits(&dma_hw->ch[DACobj[_DAC_A].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
            hw_clear_bits(&dma_hw->ch[DACobj[_DAC_B].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
            // wait for Busy flag to clear...

            // Abort the data channels...
            dma_channel_abort(DACobj[_DAC_A].data_chan);
            dma_channel_abort(DACobj[_DAC_B].data_chan);

            // Re-enable the Ctrl channels (doesn't restart data transfer)...
            hw_set_bits(&dma_hw->ch[DACobj[_DAC_A].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
            hw_set_bits(&dma_hw->ch[DACobj[_DAC_B].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);

            pio_sm_set_enabled(pio1,0,false) ;                                  // disable State machine 0 !! HARD CODED !!
            pio_sm_set_enabled(pio1,1,false) ;                                  // disable State machine 1

            for (uint i=0; i<16; i++) {                                         // Grab the GPIO back from the State machines
                gpio_init(i);
                gpio_set_dir(i, GPIO_OUT);
            }
            gpio_clr_mask(0xff) ;                                               // clear first 16 GPIO outputs

            ResultStr[0] = '\0' ;                                               // String also used as a flag, so needs to be cleared
            while (ResultStr[0] == '\0') {                                      // exit on keypress
                float Radians ;
                int outX, outY ;
                // Draw the clock face...
                for (int i=0; i<sizeof(FaceX); i++) {
                    outX=FaceX[i] * DACobj[_DAC_A].Level / 100 ;                // Scale output to match state machine settings
                    outY=FaceY[i] * DACobj[_DAC_B].Level / 100 ;
                    if (InvX) { gpio_put_masked(0x00ff,outX) ;  }               // Write inverted data byte to DAC A
                    else      { gpio_put_masked(0x00ff,255-outX) ; }            // Write non-inverted data byte to DAC A
                    if (InvY) { gpio_put_masked(0xff00,outY<<8) ; }             // Write inverted  data byte to DAC B
                    else      { gpio_put_masked(0xff00,255-outY<<8) ; }         // Write non-inverted data byte to DAC B
                    sleep_us(2) ;                                               // Pause for on-screen persistance
                }
                // Draw the clock hands...
                for (i=0; i<192; i++) {                                         // 3 hands @ 64 pixels each = 192
                    outX=HandsX[i] * DACobj[_DAC_A].Level / 100 ;               // Scale output to match state machine settings
                    outY=HandsY[i] * DACobj[_DAC_B].Level / 100 ;
                    if (InvX) { gpio_put_masked(0x00ff,outX) ;  }               // Write inverted data byte to DAC A
                    else      { gpio_put_masked(0x00ff,255-outX) ; }            // Write non-inverted data byte to DAC A
                    if (InvY) { gpio_put_masked(0xff00,outY<<8) ; }             // Write inverted  data byte to DAC B
                    else      { gpio_put_masked(0xff00,255-outY<<8) ; }         // Write non-inverted data byte to DAC B
                    sleep_us(2) ;                                               // Pause for on-screen persistance
                }

                c = getchar_timeout_us (0);                                     // Non-blocking char input
                if (c!=EOF) {                                                   // c=EOF if no input
                    if ((c=='x') or (c=='X')) {
                        InvX = !InvX ;
                        if (InvX) printf("%sX axis inverted.\n>",MarginVW) ;    // Print current status
                        else      printf("%sX axis not inverted.\n>",MarginVW) ; 
                    }
                    else if ((c=='y') or (c=='Y')) {
                        InvY = !InvY ;
                        if (InvY) printf("%sY axis inverted.\n>",MarginVW) ;    // Print current status
                        else      printf("%sY axis not inverted.\n>",MarginVW) ; 
                    }
                    if ((c=='S') or (c=='s')) {                                 // Set time
                        printf("%sSet time (format HH:MM:SS)\n%s",MarginVW, MarginFW ) ;
                        getLine() ;                                             // Get the console input
                        Parm[0]=0,  Parm[1]=0,  Parm[2]=0,  Parm[3]=0 ;         // Reset all command line parameters
                        i=0, ParmCnt=0 ;                                        // Reset all command line counters
                        while (i<strlen(inStr) ) {
                            if ((inStr[i]==':')||(inStr[i]==',')) {             // Next parameter
                                ParmCnt++ ; }
                            else if (isdigit(inStr[i])) { 
                                Parm[ParmCnt] *= 10;                            // Next digit. Bump the existing decimal digits
                                Parm[ParmCnt] += inStr[i] - '0'; }              // Convert character to integer and add
                            i++ ;                                               // Next character
                        }
                        inStr[0]='\0' ;                                         // Reset input buffer
                        Hours=Parm[0]%24 ; Mins=Parm[1]%60 ; Secs=Parm[2]%60 ;  // Set the time from parameters
                        LEDCtr=0 ;                                              // Force update and do it now
                        Repeating_Timer_Callback(&timer) ;
                        printf("\n%sClock set to %02d:%02d:%02d\n>",MarginFW,Hours,Mins,Secs) ;
                    }                    
                    else if ((c=='q') or (c=='Q')) { 
                        for (uint i=0; i<16; i++) { pio_gpio_init(pio1, i); }   // Hand the GPIO's back to the state machines

                        // Reset the data transfer DMA's to the start of the data Bitmap...
                        dma_channel_set_read_addr(DACobj[_DAC_A].data_chan, &DACobj[_DAC_A].DAC_data[0], false);
                        dma_channel_set_read_addr(DACobj[_DAC_B].data_chan, &DACobj[_DAC_B].DAC_data[0], false);
                        pio_sm_set_enabled(pio1,0,true) ;                       // Re-enable State machine 0 !! HARD CODED !!
                        pio_sm_set_enabled(pio1,1,true) ;                       // Re-enable State machine 1
                        dma_start_channel_mask(DAC_channel_mask);               // Atomic restart both DAC channels

                        strcpy(ResultStr,"        Quit clock mode\n") ;         // Prevents error message
                    }
                }
            }
        }

        // The final command is a continual loop creating the sweep function...
        if ((inStr[1] == 's') & (inStr[2] == 'w')) {                                        // Sweep
            // Parm[0]=Low frequency, Parm[1]=High frequency, Parm[2]=Scan speed, Parm[3]=Low/High pause
            i = Parm[0];
            for (;;) {
                if (SelectedChan & 0b01) result = DACobj[_DAC_A].Set(_Freq_,i) ;        // Set frequency, display status
                if (SelectedChan & 0b10) result = DACobj[_DAC_B].Set(_Freq_,i) ;        // Set frequency, display status
                dma_start_channel_mask(DAC_channel_mask);                               // Atomic restart all 4 DMA channels...
                printf(ResultStr) ;                                                     // Update terminal
                ResultStr[0] = '\0' ;                                                   // Reset the string variable
                SPI_Display_Write(i);                                                   // Update SPI display
                if (i==Parm[0]) { dirn = 1;
                                  sleep_ms(Parm[3]); }
                if (i>=Parm[1]) { dirn =-1; 
                                  sleep_ms(Parm[3]); }
                // Disable the Ctrl channels...
                hw_clear_bits(&dma_hw->ch[DACobj[_DAC_A].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                hw_clear_bits(&dma_hw->ch[DACobj[_DAC_B].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                // wait for Busy flag to clear...

                // Abort the data channels...
                dma_channel_abort(DACobj[_DAC_A].data_chan);
                dma_channel_abort(DACobj[_DAC_B].data_chan);

                // Re-enable the Ctrl channels (doesn't restart data transfer)...
                hw_set_bits(&dma_hw->ch[DACobj[_DAC_A].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                hw_set_bits(&dma_hw->ch[DACobj[_DAC_B].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);

                dma_start_channel_mask(DAC_channel_mask);                               // Atomic restart both DAC channels
                i = i + dirn;
                c = getchar_timeout_us (0);                                             // Non-blocking char input
                if ((c>=32) & (c<=126)) {
                        strcpy(ResultStr,"        Exit sweep mode\n") ;                 // Prevents error message
                        break; }                                                        // exit on keypress
                sleep_ms(Parm[2]);                                                      // Speed of scan
            }
        }

        if (strlen(ResultStr) == 0) {                                                   // No result can only mean unrecognised command
            strcpy(MarginVW,MarginFW) ;                                                 // Reset Variable Width margin
            tmp = strlen(inStr) ;
            if (tmp != 0) tmp ++ ;                                                      // Bump to allow for cursor character
            MarginVW[MWidth - tmp] = '\0' ;                                             // Calculate padding for input and cursor
            sprintf(outStr,"%sUnknown command!\n", MarginVW) ;                          // Empty response buffer indicates command has not been recognised
        }
        else strcpy(outStr,ResultStr) ;
        printf(outStr) ;                                                                // Update terminal
        outStr[0] = '\0' ;                                                              // Clear (reset) the string variable
        SPI_Display_Write(result) ;                                                     // Update SPI display
        strcpy(LastCmd, inStr) ;                                                        // Preserve last command
    }
    return 0;
}
