// Set GPIO pin assignment through compile options...
#define DEBUG                                           // Assign pins 1 and 2 to the RS-232, and direct debug output through this port
                                                        // Note: This disables the input ports on these pins (GPIO_0 and GPIO_1).
#define EIGHTBITDAC                                     // Assign pins 11 and 12 to drive the DAC.
                                                        // Note: This disables the input ports on these pins (GPIO_8 and GPIO_9).
// End of compile options

// OK, so the C++ preprocessor can't do any maths, so we have to manually expand the above selection to match the X and Y resolution...
#ifdef EIGHTBITDAC
#define DAC_Bits          8                             // Width of hardware DAC in bits.
#define BitMapSize      256                             // Match X to Y resolution
#else
#define DAC_Bits          6                             // Width of hardware DAC in bits.
#define BitMapSize       64                             // Match X to Y resolution
#endif

#include <stdio.h>
#include <math.h>
#include <cstring>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "rotary_encoder.pio.h"
#include "blink.pio.h"
#include "FastDAC.pio.h"
#include "SlowDAC.pio.h"

// Define all GPIO connections...
                                                    // Switch connections...
                                                    // ┌──────────┬────────────┬──────────────────────┐
                                                    // │ PGA2040  │ Connection │ Function             │
                                                    // ├──────────┼────────────┼──────────────────────┤
const uint SW0          =  16;                      // │ GPIO 16  │ Switch 0   │ 0=Level,1=Freq       │
const uint SW1          =  15;                      // │ GPIO 15  │ Switch 1   │ 0=Sine, 1=Square     │
const uint SW2          =  18;                      // │ GPIO 18  │ Switch 2   │ 0=Triangle, 1=Square │
const uint SW3          =  17;                      // │ GPIO 17  │ Switch 3   │ 0=Hz, 1=KHz          │
const uint SW4          =  19;                      // │ GPIO 19  │ Switch 4   │                      │
                                                    // └──────────┴────────────┴──────────────────────┘
                                                    // Nixie connections...
                                                    // Note: Cathodes - connect through a 74141 Nixie driver chip,
                                                    //       so only 4 data bits are required
                                                    // ┌──────────┬────────────┬──────────────────────┐
                                                    // │ PGA2040  │ Connection │ Function             │
                                                    // ├──────────┼────────────┼──────────────────────┤
const uint Anode_0      = 27;                       // │ GPIO 27  │ Anode 0    │ Units                │
const uint Anode_1      = 28;                       // │ GPIO 28  │ Anode 0    │ 10's                 │
const uint Anode_2      = 29;                       // │ GPIO 29  │ Anode 0    │ 100's                │
const uint Cathode_0    = 23;                       // │ GPIO 23  │ Cathode 0  │ Data bit 0           │
const uint Cathode_1    = 24;                       // │ GPIO 24  │ Cathode 0  │ Data bit 1           │
const uint Cathode_2    = 25;                       // │ GPIO 25  │ Cathode 0  │ Data bit 2           │
const uint Cathode_3    = 26;                       // │ GPIO 26  │ Cathode 0  │ Data bit 3           │
                                                    // └──────────┴────────────┴──────────────────────┘
#define SPI_PORT        spi1                        // The SPI connections will require RP2040 SPI port #1...
                                                    // ┌──────────┬────────────────┬──────────────────┐
                                                    // │ PGA2040  │ Connection     │ MCP41010         │
                                                    // ├──────────┼────────────────┼──────────────────┤
const uint PIN_SCK       = 10;                      // │ GPIO 10  │ SCK/spi1_sclk  │ SCK (pin 2)      │
const uint PIN_MOSI      = 11;                      // │ GPIO 11  │ MOSI/spi1_tx   │ SI  (pin 3)      │
const uint PIN_CS        = 12;                      // │ GPIO 12  │ Chip select    │ CS  (pin 1)      │
                                                    // └──────────┴────────────────┴──────────────────┘
const uint Onboard_LED  = 14;                       // Onboard LED
const uint EncoderClock = 21;
const uint EncoderData  = 22;
                                                    // D2A connections...
                                                    // Note: These are defined in the FastDAC.pio and SlowDAC.pio files.
                                                    //       They are only included here for completeness.
                                                    // ┌──────────┬─────────────┬─────────────────────┐
                                                    // │ PGA2040  │ Connection  │ Function            │
                                                    // ├──────────┼─────────────┤─────────────────────┤
                                                    // │ GPIO  2  │  Data bit 0 │ Least significant   │                                                   
                                                    // │ GPIO  3  │  Data bit 1 │                     │
                                                    // │ GPIO  4  │  Data bit 2 │                     │
                                                    // │ GPIO  5  │  Data bit 3 │                     │
                                                    // │ GPIO  6  │  Data bit 4 │                     │
                                                    // │ GPIO  7  │  Data bit 5 │                     │
                                                    // │ GPIO  8  │  Data bit 6 │                     │
                                                    // │ GPIO  9  │  Data bit 7 │ Most significant    │
                                                    // └──────────┴─────────────┘─────────────────────┘
// Define useful constants...
#define Slow            0
#define Fast            1
#define _Sine_          0
#define _Square_        1
#define _Triangle_      2
#define _Frequency_     0                           // For use with RotaryEnc array
#define _Level_         1
#define _WaveForm_      2

// Define GPIO lookup tables...
#ifdef DEBUG
// SW0 and SW1 assigned to RS-232 port...
//const unsigned int GPIO_Inputs[] = {SW2, SW3, SW4, SW5, SW6, EncoderClock, EncoderData};
const unsigned int GPIO_Inputs[] = {SW0, SW1, SW2, SW3, SW4, EncoderClock, EncoderData};
const unsigned GPIO_Outputs[] = {Anode_0, Anode_1, Anode_2, Cathode_0, Cathode_1, Cathode_2, Cathode_3, PIN_CS, PIN_SCK, PIN_MOSI};
#else
// SW0 and SW1 assigned as GPIO inputs...
const unsigned int GPIO_Inputs[] = {SW0, SW1, SW2, SW3, SW4, SW5, SW6, SW7, EncoderClock, EncoderData};
const unsigned GPIO_Outputs[] = {Anode_0, Anode_1, Anode_2, Cathode_0, Cathode_1, Cathode_2, Cathode_3, PIN_CS};
#endif

// Global variables...
int FreqMultiplier, ModeSelect, WaveSelect, ScanCtr, NixieVal, ScaledVal, Frequency, UpdateReq, GPIO_count;
uint PrevStatus;
int WaveForm_Type = _Sine_;
int RotaryEnc[3];                                   // Changes to the Rotary Encoder will update one of these 3 values.
                                                    // The value to be updated is determined by the current state of the application.
const uint32_t transfer_count = BitMapSize ;        // Number of DMA transfers per event

int NixieBuffer[3];                                 // Values to be displayed on Nixie tubes    - Tube0=>1's
                                                    //                                          - Tube1=>10's
                                                    //                                          - Tube2=>100's
int raw_sin[BitMapSize] ;
unsigned short DAC_data[BitMapSize] __attribute__ ((aligned(2048))) ;           // Align DAC data

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq);

class RotaryEncoder {
// Class to initialise a state machine to read the rotation of the rotary encoder
//  Based on the GitHub example here... 
//      https://github.com/GitJer/Some_RPI-Pico_stuff/tree/main/Rotary_encoder
public:
    // constructor
    // rotary_encoder_A is the pin for the A of the rotary encoder.
    // The B of the rotary encoder has to be connected to the next GPIO.
    RotaryEncoder(uint rotary_encoder_A, uint freq) {
        uint8_t rotary_encoder_B = rotary_encoder_A + 1;
        PIO pio = pio0;                                                             // Use pio 0
        uint8_t sm = 1;                                                             // Use state machine 1
        pio_gpio_init(pio, rotary_encoder_A);
        gpio_set_pulls(rotary_encoder_A, false, false);                             // configure the used pins as input without pull up
        pio_gpio_init(pio, rotary_encoder_B);
        gpio_set_pulls(rotary_encoder_B, false, false);                             // configure the used pins as input without pull up
        uint offset = pio_add_program(pio, &pio_rotary_encoder_program);            // load the pio program into the pio memory...
        pio_sm_config c = pio_rotary_encoder_program_get_default_config(offset);    // make a sm config...
        sm_config_set_in_pins(&c, rotary_encoder_A);                                // set the 'in' pins
        sm_config_set_in_shift(&c, false, false, 0);                                // set shift to left: bits shifted by 'in' enter at the least
                                                                                    // significant bit (LSB), no autopush
        irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);                     // set the IRQ handler
        irq_set_enabled(PIO0_IRQ_0, true);                                          // enable the IRQ
        pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS | PIO_IRQ0_INTE_SM1_BITS;
        pio_sm_init(pio, sm, 16, &c);                                               // init the state machine
                                                                                    // Note: the program starts after the jump table -> initial_pc = 16
        pio_sm_set_enabled(pio, sm, true);                                          // enable the state machine

        #ifdef DEBUG
        printf("PIO:0 SM:%d - Rotary encoder' @ %dHz\n\n", sm, freq);
        #endif
    }

private:
    static void pio_irq_handler() {
        if (pio0_hw->irq & 2) {                                                         // test if irq 0 was raised
             switch (ModeSelect) {
                case 0b001:                                                             // Top: Frequency range 0 to 999
                    RotaryEnc[_Frequency_]--;
                    if ( RotaryEnc[_Frequency_] < 0 ) { RotaryEnc[_Frequency_] = 999; }
                    UpdateReq |= 0b010;                                                 // Flag to update the frequency
                    break;
                case 0b010:                                                             // Bottom : Level range 0 to 99
                    RotaryEnc[_Level_]--;
                    if ( RotaryEnc[_Level_] < 0 ) { RotaryEnc[_Level_] = 99; }
                    UpdateReq |= 0b001;                                                 // Flag to update the level
                    break;
               case 0b011:                                                              // Middle: WaveForm range 0 to 4
                   RotaryEnc[_WaveForm_]--;
                   if ( RotaryEnc[_WaveForm_] < 0 ) { RotaryEnc[_WaveForm_] = 99; }
                   UpdateReq |= 0b100;                                                  // Flag to update the waveform
             }
        }
        if (pio0_hw->irq & 1) {                                                         // test if irq 1 was raised
             switch (ModeSelect) {
                case 0b001:                                                             // Top: Frequency range 0 to 999
                    RotaryEnc[_Frequency_]++;
                    if ( RotaryEnc[_Frequency_] > 999 ) { RotaryEnc[_Frequency_] = 0; }
                    UpdateReq |= 0b010;                                                 // Flag to update the frequency
                    break;
                case 0b010:                                                             // Bottom : Level range 0 to 99
                    RotaryEnc[_Level_]++;
                    if ( RotaryEnc[_Level_] > 99 ) { RotaryEnc[_Level_] = 0; }
                    UpdateReq |= 0b001;                                                 // Flag to update the level
                    break;
               case 0b011:                                                              // Middle: WaveForm range 0 to 4
                   RotaryEnc[_WaveForm_]++;
                   if ( RotaryEnc[_WaveForm_] > 99) { RotaryEnc[_WaveForm_] = 0; }
                   UpdateReq |= 0b100;                                                  // Flag to update the waveform
             }
        }
        pio0_hw->irq = 3;                                                               // clear both interrupts
    }
    PIO pio;                                                                            // the pio instance
    uint sm;                                                                            // the state machine
};

class blink_forever {                                                                   // Class to initialise a state machine to blink a GPIO pin
public:
    blink_forever(PIO pio, uint sm, uint offset, uint pin, uint freq, uint blink_div) {
        blink_program_init(pio, sm, offset, pin, blink_div);
        pio_sm_set_enabled(pio, sm, true);
        #ifdef DEBUG
        printf("PIO:0 SM:%d - Blink @ %dHz\n", sm, freq);
        #endif
    }
};

class DMAtoDAC_channel {
public:
    // Constructor
    // The PIO clock dividers are 16-bit integer, 8-bit fractional, with first-order delta-sigma for the fractional divider.
    // The clock divisor can vary between 1 and 65536, in increments of 1/256.
    // If DAC_div exceeds 2^16 (65,536), the registers wrap around, and the State Machine clock will be incorrect.
    // A slow version of the DAC State Machine is used for frequencies below 17Hz, allowing the value of DAC_div to
    // be kept within range.s    
    DMAtoDAC_channel() {
        PIO pio = pio1;
        StateMachine[Fast] = Single_DMA_FIFO_SM_GPIO_DAC(pio,Fast);             // Create the Fast DAC channel (frequencies: 17Hz to 999KHz)
        StateMachine[Slow] = Single_DMA_FIFO_SM_GPIO_DAC(pio,Slow);             // Create the Slow DAC channel (frequencies: 0Hz to 16Hz)
    }

public:
    int Single_DMA_FIFO_SM_GPIO_DAC(PIO _pio, int _speed) {
    // Create a DMA channel and its associated State Machine.
    // DMA => FIFO => State Machine => GPIO pins => DAC
        uint _pioNum = pio_get_index(_pio);                                     // Get user friendly index number.
        int _offset;
        char _name[10];
        uint _StateMachine = pio_claim_unused_sm(_pio, true);                   // Find a free state machine on the specified PIO - error if there are none.

        if (_speed == 1) {
        // Configure the state machine to run the FastDAC program...
            _offset = pio_add_program(_pio, &pio_FastDAC_program);              // Use helper function included in the .pio file.
            pio_FastDAC_program_init(_pio, _StateMachine, _offset, 2);
            strcpy(_name,"Fast");
        } else {
        // Configure the state machine to run the SlowDAC program...
            _offset = pio_add_program(_pio, &pio_SlowDAC_program);              // Use helper function included in the .pio file.
            pio_SlowDAC_program_init(_pio, _StateMachine, _offset, 2);
            strcpy(_name,"Slow");
        }
        //  Get 2 x free DMA channels for the DAC - panic() if there are none
        int ctrl_chan = dma_claim_unused_channel(true);
        int data_chan = dma_claim_unused_channel(true);

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
        channel_config_set_dreq(&fc, pio_get_dreq(pio, _StateMachine, true));   // Transfer when PIO SM TX FIFO has space
        channel_config_set_chain_to(&fc, ctrl_chan);                            // chain to the controller DMA channel
#ifdef EIGHTBITDAC        
        channel_config_set_ring(&fc, false, 9);                                 // 8 bit DAC 1<<9 byte boundary on read ptr. This is why we needed alignment!
#else
        channel_config_set_ring(&fc, false, 7);                                 // 6 bit DAC 1<<7 byte boundary on read ptr. This is why we needed alignment!
#endif
        dma_channel_configure(
            data_chan,                                                          // Channel to be configured
            &fc,                                                                // The configuration we just created
            &pio->txf[_StateMachine],                                           // Write to FIFO
            DAC_data,                                                           // The initial read address (AT NATURAL ALIGNMENT POINT)
            BitMapSize,                                                         // Number of transfers; in this case each is 2 byte.
            false                                                               // Don't start immediately.
        );
        // Note: Both DMA channels are left permanently running. The active channel is selected by enabling/disabling the 
        // associated State Machine.
        dma_start_channel_mask(1u << ctrl_chan);                                // Start the control DMA channel

        #ifdef DEBUG
        printf("%s DMA channel:\n", _name);
        printf("    PIO:              %d\n",_pioNum);
        printf("    State machine:    %d\n",_StateMachine);
        printf("    Program offset:   %d\n",_offset);
        printf("    DMA Ctrl channel: %d\n",ctrl_chan);
        printf("    DMA Data channel: %d\n",data_chan);
        #endif

        return(_StateMachine);
}

// Setter functions...
void Set_Frequency(int _frequency){
    // If DAC_div exceeds 2^16 (65,536), the registers wrap around, and the State Machine clock will be incorrect.
    // A slow version of the DAC State Machine is used for frequencies below 17Hz, allowing the value of DAC_div to
    // be kept within range.
    float DAC_freq = _frequency * BitMapSize;                                   // Target frequency...
    float DAC_div = 2 * (float)clock_get_hz(clk_sys) / DAC_freq;                // ...calculate the PIO clock divider required for the given Target frequency
    float Fout = 2 * (float)clock_get_hz(clk_sys) / (BitMapSize * DAC_div);     // Actual output frequency
    if (_frequency >= 34) {                                                     // Fast DAC ( Frequency range from 34Hz to 999Khz )
        pio_sm_set_clkdiv(pio, StateMachine[Fast], DAC_div);                    // Set the State Machine clock speed
        pio_sm_set_enabled(pio, StateMachine[Fast], true);                      // Fast State Machine active
        pio_sm_set_enabled(pio, StateMachine[Slow], false);                     // Slow State Machine inactive
        #ifdef DEBUG        
        printf("Rotation: %03d - Fast SM - SM Div: %8.4f - SM Clk: %07.0gHz - Fout: %.1f",_frequency, DAC_div, DAC_freq, Fout);
        #endif
    } else {                                                                    // Slow DAC ( 1Hz=>16Hz )
        DAC_div = DAC_div / 64;                                                 // Adjust DAC_div to keep within useable range
        DAC_freq = DAC_freq * 64;
        pio_sm_set_clkdiv(pio, StateMachine[Slow], DAC_div);                    // Set the State Machine clock speed
        pio_sm_set_enabled(pio, StateMachine[Fast], false);                     // Fast State Machine inactive
        pio_sm_set_enabled(pio, StateMachine[Slow], true);                      // Slow State Machine active
        #ifdef DEBUG        
        printf("Rotation: %03d - Slow SM - SM Div: %8.4f - SM Clk: %07.0gHz - Fout: %.1f",_frequency, DAC_div, DAC_freq, Fout);
        #endif
    }
    #ifdef DEBUG    
    if (_frequency < 1000) { printf("Hz\n"); } else { printf("KHz\n"); }
    #endif
}

//static int offset;
PIO pio = pio1;
static uint StateMachine[2];
};
// Global Var...
uint DMAtoDAC_channel::StateMachine[2];

void WriteCathodes (int Data) {
// Create bit pattern on cathode GPIO's corresponding to the Data input...
    int  shifted;
    shifted = Data ;
    gpio_put(Cathode_0, shifted %2) ;
    shifted = shifted /2 ;
    gpio_put(Cathode_1, shifted %2);
    shifted = shifted /2;
    gpio_put(Cathode_2, shifted %2);
    shifted = shifted /2;
    gpio_put(Cathode_3, shifted %2);
}

bool Repeating_Timer_Callback(struct repeating_timer *t) {
// Scans the Nixie Anodes, and transfers data from the Nixie Buffers to the Cathodes.
    switch (ScanCtr) {
        case 0:
            gpio_put(Anode_2, 0) ;                                      // Turn off previous anode
            WriteCathodes(NixieBuffer[0]);                              // Set up new data on cathodes (Units)
            gpio_put(Anode_0, 1) ;                                      // Turn on current anode
            break;
        case 1:
            gpio_put(Anode_0, 0) ;                                      // Turn off previous anode
            WriteCathodes(NixieBuffer[1]);                              // Set up new data on cathodes (10's)
            gpio_put(Anode_1, 1) ;                                      // Turn on current anode
            break;
        case 2:
            gpio_put(Anode_1, 0) ;                                      // Turn off previous anode
            WriteCathodes(NixieBuffer[2]);                              // Set up new data on cathodes (100's)
            gpio_put(Anode_2, 1) ;                                      // Turn on current anode.
    }
    ScanCtr++;
    if ( ScanCtr > 2 ) { ScanCtr = 0; }                                 // Bump and Wrap the counter
    return true;
}

void WaveForm_Update(int _WaveForm_Type, int _WaveForm_Value) {
    int i;
    int offset = BitMapSize/2 - 1;                                                                        // Shift sine waves up above X axis
    const float _2Pi = 6.283;                                                                             // 2*Pi
    float a,b,x1,x2,g1,g2;
    switch (_WaveForm_Type) {
        case _Sine_:
            _WaveForm_Value = _WaveForm_Value % 64;                                                        // Sine value cycles after 7
            #ifdef DEBUG
            printf("Sine wave: Fundamental + %d harmonics.\n",_WaveForm_Value);
            #endif
            for (i=0; i<BitMapSize; i++) {
                a = offset * sin((float)_2Pi*i / (float)BitMapSize);                                      // Fundamental frequency...
                if (_WaveForm_Value >= 1) { a += offset/3  * sin((float)_2Pi*3*i  / (float)BitMapSize); } // Add  3rd harmonic
                if (_WaveForm_Value >= 2) { a += offset/5  * sin((float)_2Pi*5*i  / (float)BitMapSize); } // Add  5th harmonic
                if (_WaveForm_Value >= 3) { a += offset/7  * sin((float)_2Pi*7*i  / (float)BitMapSize); } // Add  7th harmonic
                if (_WaveForm_Value >= 4) { a += offset/9  * sin((float)_2Pi*9*i  / (float)BitMapSize); } // Add  9th harmonic
                if (_WaveForm_Value >= 5) { a += offset/11 * sin((float)_2Pi*11*i / (float)BitMapSize); } // Add 11th harmonic
                if (_WaveForm_Value >= 6) { a += offset/13 * sin((float)_2Pi*13*i / (float)BitMapSize); } // Add 13th harmonic
                if (_WaveForm_Value >= 7) { a += offset/15 * sin((float)_2Pi*15*i / (float)BitMapSize); } // Add 15th harmonic
                DAC_data[i] = (int)(a)+offset;                                                            // Sum all harmonics and add vertical offset
            }          
            break;
        case _Square_: 
            #ifdef DEBUG
            printf("Square wave: %2d%% duty cycle\n",_WaveForm_Value);
            #endif
            b = _WaveForm_Value * BitMapSize / 100;                                                       // Convert % to value
            for (i=0; i<BitMapSize; i++) {
                if (b <= i) { DAC_data[i] = 0;   }                                                        // First section low
                else        { DAC_data[i] = 255; }                                                        // Second section high
            }
            break;
        case _Triangle_: 
            #ifdef DEBUG
            printf("Triangle wave %2d%% duty cycle\n",_WaveForm_Value);
            #endif
            x1 = (_WaveForm_Value * BitMapSize / 100) -1;                                                 // Number of data points to peak
            x2 = BitMapSize - x1;                                                                         // Number of data points after peak
            g1 = (BitMapSize - 1) / x1;                                                                   // Rising gradient (Max val = BitMapSize -1)
            g2 = (BitMapSize - 1) / x2;                                                                   // Falling gradient (Max val = BitMapSize -1)
            for (i=0; i<BitMapSize; i++) {
                if (i <= x1) { DAC_data[i] = i * g1; }                                                    // Rising  section of waveform...
                if (i > x1)  { DAC_data[i] = (BitMapSize - 1) - ((i - x1) * g2); }                        // Falling section of waveform
            }
            break;
    }

    // finished with _WaveForm_Value, so ok to trash it as we update the display...    
    NixieBuffer[0] = _WaveForm_Value % 10 ;                                                               // First Nixie ( 1's )
    _WaveForm_Value /= 10 ;                                                                               // _value=>10's
    NixieBuffer[1] = _WaveForm_Value % 10 ;                                                               // Second Nixie ( 10's )
    _WaveForm_Value /= 10 ;                                                                               // _value=>100's
    NixieBuffer[2] = 10 ;                                                                                 // Blank Third Nixie ( 100's )
}

static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 0);                                                        // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 1);
    asm volatile("nop \n nop \n nop");
}

static void MCP41010_write(int _data) {
// Formats and transmits 16 bit data word to the MCP41010 digital potentiometer...
    uint8_t buff[2];
    buff[0] = 0x11;                                                             // Control byte: Write to potentiometer #1
    buff[1] = _data;                                                            // Data byte
    cs_select();
    spi_write_blocking(SPI_PORT, buff, 2);
    cs_deselect();
}

void gpio_callback(uint gpio, uint32_t events) {
    volatile bool CurBit,PrevBit;
    volatile uint SwitchStatus = 0;
    volatile uint BitMask = 1 << GPIO_count-2;
//  printf("Previous Status: %b\n",PrevStatus);
// Scan down through input ports and create status bitmap...
    for (int i=GPIO_count-2; i>=0 ; i--) {                                      // Note: 'GPIOCount-2' skips encoder pins
        SwitchStatus <<= 1;                                                     // Bit shift left
        CurBit = gpio_get(GPIO_Inputs[i]);
        PrevBit = PrevStatus & BitMask;
        SwitchStatus += CurBit;
//      printf("Bit %d - Masked=%08b value=%b SwitchStatus=%08b\n",i,PrevBit,CurBit,SwitchStatus);
        if (PrevBit != CurBit) {
            // Do stuff here...
            switch (i) {
                case 0:
                    if (CurBit) { printf("Frequency\n");
                                  ModeSelect = 0b0001;
                                  UpdateReq = 0b010; }
                    else        { printf("Level\n"); 
                                  ModeSelect = 0b0010;
                                  UpdateReq = 0b001; }              // Flag to update the level
                    break;
                case 1:
                    if (CurBit) { printf("Square\n"); 
                                  WaveForm_Type = _Square_ ;
                                  RotaryEnc[_WaveForm_] = 50;       // Set default: 50% duty cycle
                                  ModeSelect = 0b011; }
                    else        { printf("Sine\n"); 
                                  WaveForm_Type = _Sine_ ;
                                  RotaryEnc[_WaveForm_] = 0;        // Set default: Sine wave, no harmonics
                                  ModeSelect = 0b011; }
                    UpdateReq = 0b100;                              // Flag to update the waveform
                    break;
                case 2:
                    if (CurBit) { printf("Square\n");
                                  WaveForm_Type = _Square_ ;
                                  RotaryEnc[_WaveForm_] = 50;       // Set default: 50% duty cycle
                                  ModeSelect = 0b011; }
                    else        { printf("Triangle\n");
                                  WaveForm_Type = _Triangle_ ;
                                  RotaryEnc[_WaveForm_] = 50 ;      // Set default: 50% duty cycle
                                  ModeSelect = 0b011; }
                    UpdateReq = 0b100;                              // Flag to update the waveform
                    break;
                case 3:
                    if (CurBit) { printf("Hz\n"); 
                                  FreqMultiplier = 1; }
                    else        { printf("KHz\n"); 
                                  FreqMultiplier = 1000; }
                    UpdateReq = 0b010;                              // Flag to update the frequency
                    break;
            }
//          printf("Bit %d / Switch %d / GPIO %2d has changed. %b->%b\n",i,i+2,GPIO_Inputs[i],PrevBit,CurBit);
        }
        BitMask >>= 1;                                                       // Next bit
    }
//  printf("Current Status:  %b\n\n",SwitchStatus);
    PrevStatus = SwitchStatus;
}

int main() {
    static const float blink_freq = 16000;                                      // Reduce SM clock to keep flash visible...
    static const float rotary_freq = 16000;                                     // Clock speed reduced to eliminate rotary encoder jitter...
//  set_sys_clock_khz(280000, true);                                            // Overclocking the core by a factor of 2 allows 1MHz from DAC
    float blink_div = (float)clock_get_hz(clk_sys) / blink_freq;                //   ... calculate the required blink SM clock divider
    float rotary_div = (float)clock_get_hz(clk_sys) / rotary_freq;              //... then calculate the required rotary encoder SM clock divider

#ifdef DEBUG
    stdio_init_all();                                                           // Needed for printf
#endif

// Initialise GPIO Outputs...
    GPIO_count = sizeof(GPIO_Outputs)/sizeof(*GPIO_Outputs);
    for ( uint i = 0; i < GPIO_count; i++ ) {
        gpio_init(GPIO_Outputs[i]);
        gpio_set_dir(GPIO_Outputs[i], GPIO_OUT);
    }
    gpio_put(PIN_CS, 1);                                                        // SPI chip select is active-low, so set to inactive state

/* //Initialise PIO Outputs for DAC...
    for ( uint i = 0; i < DAC_Bits; i++ ) {
        gpio_set_slew_rate(GPIOvals[i+2],GPIO_SLEW_RATE_FAST);                  // GPIO Warp factor 10
        gpio_set_drive_strength(GPIOvals[i+2],GPIO_DRIVE_STRENGTH_12MA);
    } */

// Initialise GPIO Inputs...
// TBD - DO I WANT PULL UPS ON THE ENCODER PINS ???
    GPIO_count = sizeof(GPIO_Inputs)/sizeof(*GPIO_Inputs);
    for ( uint i = 0; i < GPIO_count; i++ ) {
        gpio_init(GPIO_Inputs[i]);
        gpio_set_dir(GPIO_Inputs[i], GPIO_IN);
        gpio_pull_up(GPIO_Inputs[i]);                                           // Enable pull up
    }
// Enable GPIO interupts...
    gpio_set_irq_enabled_with_callback(SW0, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(SW1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(SW2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(SW3, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(SW4, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

// Set SPI0 to 0.5MHz...
    spi_init(SPI_PORT, 500 * 1000);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    RotaryEncoder my_encoder(EncoderClock, rotary_freq);

// Confirm memory alignment
    #ifdef DEBUG
    printf("Confirm memory alignment...\nBeginning: %x", &DAC_data[0]);
    printf("\nFirst: %x", &DAC_data[1]);
    printf("\nSecond: %x\n", &DAC_data[2]);
    int tmp = BitMapSize;
    printf("Size (bytes): %d\n\n",tmp);
    #endif

// Set up the State machines...
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &pio_blink_program);
    blink_forever my_blinker(pio, 0, offset, Onboard_LED, blink_freq, blink_div);   // SM0=>onboard LED

    DMAtoDAC_channel DataChannel;                                                   // Create DMAtoDAC_channel object

// Create a repeating timer that calls Repeating_Timer_Callback.
// If the delay is > 0 then this is the delay between the previous callback ending and the next starting. If the delay is negative
// then the next call to the callback will be exactly 7ms after the start of the call to the last callback.
  struct repeating_timer timer;
    add_repeating_timer_ms(-7, Repeating_Timer_Callback, NULL, &timer);             // 7ms - Short enough to prevent Nixie tube flicker
                                                                                    //       Long enough to prevent Nixie tube bluring

    RotaryEnc[_Frequency_] = 100;                                                   // Default: 100Hz
    RotaryEnc[_Level_] = 50;                                                        // Default: 50%
    RotaryEnc[_WaveForm_] = 0;                                                      // Default: Sine wave, no harmonics

// GPIO interrupt routine is triggerd at start up, but has no 'previous state' info. This means we have to manualy set a couple of defaults...
// TBD - WHY NOT READ THE SWITCHES ????
    WaveForm_Type = _Sine_ ;
    FreqMultiplier = 1;                                                             // Default: Hz
    ModeSelect = 0b0001;
    UpdateReq = 0b0111;                                                             // Set flags to load all default values

    while (true) {                                                                  // Infinite loop
        if (UpdateReq) {
        // Falls through here when any of the rotary encoder values change...
            if (UpdateReq & 0b010) {                                                // Frequency has changed
                NixieVal = RotaryEnc[_Frequency_];                                  // Value in range 0->999
                Frequency = NixieVal * FreqMultiplier;
                DataChannel.Set_Frequency(Frequency);

                NixieBuffer[0] = NixieVal % 10 ;                                    // First Nixie ( 1's )
                NixieVal /= 10 ;                                                    // finished with NixieVal, so ok to trash it. NixieVal=>10's
                NixieBuffer[1] = NixieVal % 10 ;                                    // Second Nixie ( 10's )
                NixieVal /= 10 ;                                                    // NixieVal=>100's
                NixieBuffer[2] = NixieVal % 10 ;                                    // Third Nixie ( 100's )
            }
            if (UpdateReq & 0b100) {                                                // Waveform has changed
                NixieVal = RotaryEnc[_WaveForm_];
                WaveForm_Update(WaveForm_Type, NixieVal);
                NixieBuffer[0] = NixieVal % 10 ;                                    // First Nixie ( 1's )
                NixieVal /= 10 ;                                                    // finished with NixieVal, so ok to trash it. NixieVal=>10's
                NixieBuffer[1] = NixieVal % 10 ;                                    // Second Nixie ( 10's )
                NixieVal /= 10 ;                                                    // NixieVal=>100's
                NixieBuffer[2] = 10 ;                                               // Blank Third Nixie ( 100's )
            }
            if (UpdateReq & 0b001) {                                                // Level has changed
                NixieVal  = RotaryEnc[_Level_];
                ScaledVal = NixieVal*255/99;                                        // Scale the level. Display: 0->99 - Potentiometer: 0->255
                #ifdef DEBUG
                printf("Level: %02d%% Level(Abs): %d\n",NixieVal,ScaledVal);
                #endif
                MCP41010_write(ScaledVal);                                          // Send over SPI to digital potentiometer
                NixieBuffer[0] = NixieVal % 10 ;                                    // First Nixie ( 1's )
                NixieVal /= 10 ;                                                    // finished with teNixieValmp, so ok to trash it. NixieVal=>10's
                NixieBuffer[1] = NixieVal % 10 ;                                    // Second Nixie ( 10's )
                NixieVal /= 10 ;                                                    // NixieVal=>100's
                NixieBuffer[2] = 10 ;                                               // Blank Third Nixie ( 100's )
            }
            UpdateReq = 0;                                                          // All up to date, so clear the flag
        }
    }
}
