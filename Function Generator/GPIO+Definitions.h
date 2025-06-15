#pragma once

#define NewHardware               // Comment/Uncomment to select active GPIO ports

#ifdef NewHardware
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Define GPIO connections for Pico ** with UART ** ...
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Phase lock: 
//      Read/Write through serial port does not use DMA channels. This removes most DAC channels jitter at
//      high frequencies.
//          1  Hz => 900 KHz - Phase sync stable
//        900 KHz => 1 MHz   - Phase sync usable - random drifting easily corrected by hitting 'return'
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SPI_PORT        spi0                        // Using SPI port 0
                                                    // ┌──────────┬───────────────┬─────────────┐────────────────┐
                                                    // │ PGA2040  │ Connection    │ MCP41010    │ Display module │
                                                    // ├──────────┼───────────────┼─────────────┤────────────────┤
#define PIN_RX          12                          // │ GPIO 12  │ SPI1 RX       │ (unused)    │  (unused)      │
#define Display_CS      27                          // │ GPIO 27  │ Chip select   │ (unused)    │  SS1 (white)   │
#define PIN_CLK          2                          // │ GPIO  2  │ SPI1 Clock    │ SCK (Pin 2) │  SCK (blue)    │
#define PIN_TX           3                          // │ GPIO  3  │ SPI1 TX       │ SI  (Pin 3) │  SDI (green)   │
#define Level_CS        26                          // │ GPIO 26  │ Chip select   │ CS  (Pin 1) │  (unused)      │
//                                                     └──────────┴───────────────┴─────────────┘────────────────┘
#define DAC_A_Start     15                          // First GPIO port used by DAC A
#define DAC_B_Start      7                          // First GPIO port used by DAC B
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#else
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Define GPIO connections for Pico ** NO UART ** ...
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Phase lock: 
//      Read/Write through USB serial port requires DMA channel usage. This makes the DAC channels jitter at
//      high frequencies and causes phase sync issues.
//          1  Hz => 250 KHz - Phase sync stable
//        250 KHz => 750 KHz - Phase sync usable   - some drifting noticable when using USB serial input
//        750 KHz => 1 MHz   - Phase sync unusable - random drifting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Note: 1) The SPI Port only works through specific pins, so these connections are defined first.
//       2) Pin assignments prevent the use of the UART for this configuration.
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
//                                                     └──────────┴───────────────┴─────────────┘────────────────┘
#define DAC_A_Start      0                          // First GPIO port used by DAC A
#define DAC_B_Start      8                          // First GPIO port used by DAC B
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif

// Definitions common to either hardware...
#define _DAC_A           0                          // DAC channel alias
#define _DAC_B           1                          // DAC channel alias
#define _Funct_          2
#define _Phase_          3
#define _Duty_           4
#define _Range_          5
#define _Harmonic_      10
#define _PIOnum_        pio0                        // Code will work equally well on either pio0 or pio1
#define eof            255                          // EOF in stdio.h -is -1, but getchar returns int 255 to avoid blocking
#define eot              3                          // End Of Text
#define CR              13
#define MWidth          12                          // Width of terminal command margin (in columns)
#define Increment        1
#define Decrement       -1

// Define clock speed (select one set from below)...
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//#define SysClock       125                        // System clock @ 125MHz (Pico default)
//#define MaxFreq     488000                        //  ...for 0.488 MHz DAC output
//
//#define SysClock       250                        // System clock x 2
//#define MaxFreq     977000                        //  ...for 0.977 MHz DAC output
//
#define SysClock       280                          // Overclock @ 280MHz...
#define MaxFreq    1000000                          //  ...for 1.00MHz DAC output
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
