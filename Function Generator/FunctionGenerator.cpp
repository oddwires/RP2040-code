#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include <math.h>
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "blink.pio.h"
#include "DAC.pio.h"

//////////////////////////////////////
// Define GPIO connections for Pico...
//////////////////////////////////////

// Note: The SPI Port only works through specific pins, so this port is defined first.
// SPI Port connections...                          // ┌──────────┬───────────────┬─────────────┐────────────────┐
                                                    // │ PGA2040  │ Connection    │ MCP41010    │ Display module │
                                                    // ├──────────┼───────────────┼─────────────┤────────────────┤
/* #define PIN_RX          16                          // │ GPIO 16  │ RX/spi1_rx    │             │      -         │
//#define PIN_CS          17                        // │ GPIO 17  │ CS/spi1_cs    │             │                │ can this be re-defined ?
#define PIN_CLK         18                          // │ GPIO 18  │ CLK/spi1_clk  │             │  SCK (blue)    │
#define PIN_TX          19                          // │ GPIO 19  │ TX/spi1_tx    │             │  SDI (green)   │
#define Display_CS        21                        // │ GPIO 21  │ Chip select   │             │  SS1 (white)   │ */

#define PIN_RX           4                          // │ GPIO  4  │ RX/spi1_rx    │             │      -         │
//#define PIN_CS          17                        // │ GPIO 17  │ CS/spi1_cs    │             │                │ can this be re-defined ?
#define PIN_CLK          2                          // │ GPIO  2  │ CLK/spi1_clk  │             │  SCK (blue)    │
#define PIN_TX           3                          // │ GPIO  3  │ TX/spi1_tx    │             │  SDI (green)   │
#define Display_CS       6                          // │ GPIO  6  │ Chip select   │             │  SS1 (white)   │
                                                    // └──────────┴───────────────┴─────────────┘────────────────┘
#define SPI_PORT        spi0                        // These SPI connections require the use of RP2040 SPI port 0

#define _A               0                           // DAC channel alias
#define _B               1
#define _Up              1
#define _Down           -1
#define _Sine_           0                          // Permited values for variable WaveForm_Type
#define _Square_         1
#define _Triangle_       2
#define _DMA_ctrl_       6
#define _DMA_data_       7
#define _Funct_          8
#define _Phase_          9
#define _Freq_          10
#define _Duty_          11
#define _Range_         12
#define eof            255                          // EOF in stdio.h -is -1, but getchar returns int 255 to avoid blocking
#define CR              13
#define BitMapSize     256                          // Match X to Y resolution
//#define BitMapSize      360                       // won't work - DMA needs to operate as a power of 2

unsigned short DAC_channel_mask = 0 ;               // Binary mask to simultaneously start all DMA channels
const uint32_t transfer_count = BitMapSize ;        // Number of DMA transfers per event
int ParmCnt = 0, Parm[4], WaveForm_Type ;           // Storage for 4 command line parameters
int SelectedChan, c, i = 0, dirn = 1, result ;
char inStr[30], outStr[2048], LastCmd[30] ;         // outStr large enough to contain the HelpText string
const char * HelpText = 
"\tUsage...\n"
"\t  ?            - Usage\n"
"\t  S            - Status\n"
"\t  R            - Resource Allocation\n"
"\t  <A/B/C>h     - Frequency multiplier  Hz\n"
"\t  <A/B/C>k     - Frequency multiplier KHz\n"
"\t  <A/B/C>si    - Sine wave\n"
"\t  <A/B/C>sq    - Square wave\n"
"\t  <A/B/C>tr    - Triangle wave\n"
"\t  <A/B/C>sw    - Sweep frequency (Low, High, Speed, Pause)\n"
"\t  <A/B/C>frnnn - Frequency = nnn            ( 0->999 )\n"
"\t  <A/B/C>fr+   - Frequency + 1\n"
"\t  <A/B/C>fr-   - Frequency - 1\n"
"\t  <A/B/C>phnnn - Phase = nnn                ( 0->359 degrees )\n"
"\t  <A/B/C>ph+   - Phase + 1\n"
"\t  <A/B/C>ph-   - Phase - 1\n"
"\t  <A/B/C>dunnn - Duty Cycle = nnn           ( 0->100%% )\n"
"\t  <A/B/C>du+   - Duty Cycle + 1\n"
"\t  <A/B/C>du-   - Duty Cycle - 1\n"
"\twhere...\n"
"\t  <A/B/C> = DAC channel A,B or Both\n"
"\t  nnn     = Three digit numeric value\n";

class DAC {
public:
    PIO pio;                                                                // Class wide var to share value with setter function
    unsigned short DAC_data[BitMapSize] __attribute__ ((aligned(2048))) ;   // Align DAC data (2048d = 0800h)
    int Phase, Funct, Freq, Range, DutyC, PIOnum ;
    uint StateMachine, ctrl_chan, data_chan, GPIO, SM_WrapBot, SM_WrapTop ; // Variabes used by the getter function...
    char name ;                                                             // Name of this instance
    float DAC_div ;

    void StatusString () {
    // Print the status line for the current DAC object.
        char Str1[4], Str2[50] ;                                            // !  Max line length = 50 chars !
        Range == 1 ? strcpy(Str1,"Hz") : strcpy(Str1,"KHz") ;               // Asign multiplier suffix
            switch ( Funct ) {                                              // Calculate status sting...
                case _Sine_:
                    sprintf(Str2,"\tChannel %c: Freq:%03d%s Phase:%03d  Wave:Sine\n", name, Freq, Str1, Phase) ;
                    break;
                case _Triangle_:
                    if ((DutyC == 0) || (DutyC == 100)) {
                        sprintf(Str2,"\tChannel %c: Freq:%03d%s Phase:%03d  Wave:Sawtooth\n", name, Freq, Str1, Phase) ;
                    } else {
                        sprintf(Str2,"\tChannel %c: Freq:%03d%s Phase:%03d  Wave:Triangle Rise time:%d%%\n", name, Freq, Str1, Phase, DutyC) ;
                    }
                break;
                case _Square_:
                    sprintf(Str2,"\tChannel %c: Freq:%03d%s Phase:%03d  Wave:Square Duty cycle:%d%%\n", name, Freq, Str1, Phase, DutyC) ;
            }
        strcat(outStr,Str2) ;
    }

    // Setter functions...
    void ReInit () {
    // Re-initialises DMA channels to their initial state.
    // Note: 1) DMA channels are not restarted, allowing for atomic (simultaneous) restart of both DAC channels later.
    //       2) Cannot use dma_hw->abort on chained DMA channels, so using disable and re-enable instead.
    //       3) This needs to be performed across both DAC channels to ensure phase sync is maintained.
    // Disable both DMA channels associated with this DAC...
        hw_clear_bits(&dma_hw->ch[data_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
        hw_clear_bits(&dma_hw->ch[ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);        
    // Reset the data transfer DMA's to the start of the data Bitmap...
        dma_channel_set_read_addr(data_chan, &DAC_data[0], false);
    // Re-enable both DMA channels associated with this DAC...
        hw_set_bits(&dma_hw->ch[data_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
        hw_set_bits(&dma_hw->ch[ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    }
    
    int Set(int _type, int _val) {
    // _type = Frequency / Phase / Duty, _dirn = Up / Down (_Up = 1, _Down = -1)
        if (_type == _Freq_) {
            Freq  = _val ;                              // Frequency (numeric)
            ReInit() ;                                  // Stop and reset the DAC channel (no restart)
            DACspeed(Freq * Range) ; }                  // Update State machine run speed
        if (_type == _Range_) {
            Range  = _val ;                             // Frequency (multiplier)
            ReInit() ;                                  // Stop and reset the DAC channel (no restart)
            DACspeed(Freq * Range) ; }                  // Update State machine run speed
        if (_type == _Phase_) {
            Phase  = _val ;                             // Phase shift (0->355 degrees)
            ReInit() ;                                  // Stop and reset the DAC channel (no restart)
            DataCalc() ; }                              // Recalc Bitmap and apply new phase value
        if (_type == _Duty_) {
            DutyC = _val ;                              // Duty cycle  (0->100%)
            DataCalc() ; }                              // Recalc Bitmap and apply new Duty Cycle value
        if (_type == _Funct_) {                         // Function    (Sine/Triangl/Square)
            Funct = _val ;
            DataCalc() ;                                // Recalc Bitmap and apply new Function value
        }
        return (_val) ;
    }

    int Bump(int _type, int _dirn) {
    // _type = Frequency / Phase / Duty, _dirn = Up / Down (_Up = 1, _Down = -1)
        int val = 0 ;
        if (_type == _Freq_) {
            Freq += _dirn ;
            if (Freq >= 1000) Freq = 0 ;                // Endwrap
            if (Freq < 0)     Freq = 999 ;              // Endwrap
            val = Freq ;
            DACspeed(Freq * Range) ;  }
        if (_type == _Phase_) {
            Phase += _dirn ;
            if (Phase == 360)  Phase = 0 ;              // Endwrap
            if (Phase  < 0  )  Phase = 359 ;            // Endwrap
            val = Phase ;
            DataCalc(); }                               // Update Bitmap data to include new DAC phase
        if (_type == _Duty_) {
            DutyC += _dirn ;
            if (DutyC > 100) { DutyC = 0 ;   }          // Top endwrap
            if (DutyC < 0  ) { DutyC = 100 ; }          // Bottom endwrap
            val = DutyC ;
            DataCalc(); }                               // Update Bitmap with new Duty Cycle value
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
        StatusString () ;                                                        // Update the terminal session
    }

    void DataCalc () {
    //    int i,h_index, v_offset = BitMapSize/2 - 1;                                                 // Shift sine waves up above X axis
        int i,j, v_offset = 256/2 - 1;                                                                // Shift sine waves up above X axis
        int _phase;
        const float _2Pi = 6.283;                                                                     // 2*Pi
        float a,b,x1,x2,g1,g2;

    // Scale the phase shift to match data size...    
        _phase = Phase * BitMapSize / 360 ;                                                            // Input  range: 0 -> 360 (degrees)
                                                                                                      // Output range: 0 -> 255 (bytes)
        switch (Funct) {
            case _Sine_:
                DutyC = DutyC % 10;                                                                   // Sine value cycles after 7
                for (i=0; i<BitMapSize; i++) {
                // Add the phase offset and wrap data beyond buffer end back to the buffer start...
                    j = ( i + _phase ) % BitMapSize;                                                  // Horizontal index
                    a = v_offset * sin((float)_2Pi*i / (float)BitMapSize);                            // Fundamental frequency...
                    if (DutyC >= 1) { a += v_offset/3  * sin((float)_2Pi*3*i  / (float)BitMapSize); } // Add  3rd harmonic
                    if (DutyC >= 2) { a += v_offset/5  * sin((float)_2Pi*5*i  / (float)BitMapSize); } // Add  5th harmonic
                    if (DutyC >= 3) { a += v_offset/7  * sin((float)_2Pi*7*i  / (float)BitMapSize); } // Add  7th harmonic
                    if (DutyC >= 4) { a += v_offset/9  * sin((float)_2Pi*9*i  / (float)BitMapSize); } // Add  9th harmonic
                    if (DutyC >= 5) { a += v_offset/11 * sin((float)_2Pi*11*i / (float)BitMapSize); } // Add 11th harmonic
                    if (DutyC >= 6) { a += v_offset/13 * sin((float)_2Pi*13*i / (float)BitMapSize); } // Add 13th harmonic
                    if (DutyC >= 7) { a += v_offset/15 * sin((float)_2Pi*15*i / (float)BitMapSize); } // Add 15th harmonic
                    if (DutyC >= 8) { a += v_offset/17 * sin((float)_2Pi*17*i / (float)BitMapSize); } // Add 17th harmonic
                    if (DutyC >= 9) { a += v_offset/19 * sin((float)_2Pi*19*i / (float)BitMapSize); } // Add 19th harmonic
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
                x1 = (DutyC * BitMapSize / 100) -1;                                                   // Number of data points to peak
                x2 = BitMapSize - x1;                                                                 // Number of data points after peak
                g1 = (BitMapSize - 1) / x1;                                                           // Rising gradient (Max val = BitMapSize -1)
                g2 = (BitMapSize - 1) / x2;                                                           // Falling gradient (Max val = BitMapSize -1)
                for (i=0; i<BitMapSize; i++) {
                    j = ( i + _phase ) % BitMapSize;                                                  // Horizontal index
                    if (i <= x1) { DAC_data[j] = i * g1; }                                            // Rising  section of waveform...
                    if (i > x1)  { DAC_data[j] = (BitMapSize - 1) - ((i - x1) * g2); }                // Falling section of waveform
                }
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
        Funct = _Sine_, Freq = 100, Range = 1, DutyC = 50 ;                     // Assign start-up default values.
        name == 'A' ? Phase = 0 : Phase = 180 ;                                 // Phase difference between channels
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

class blink_forever {                                                      // Class to initialise a state machine to blink a GPIO pin
PIO pio ;                                                                   // Class wide variables to share value with setter function
public:
uint pioNum, StateMachine, Freq, _offset ;
    blink_forever(PIO _pio) {
        pio = _pio;                                                        // transfer parameter to class wide var
        pioNum = pio_get_index(_pio);
        StateMachine = pio_claim_unused_sm(_pio, true);                    // Find a free state machine on the specified PIO - error if there are none.
        _offset = pio_add_program(_pio, &pio_blink_program);
        blink_program_init(_pio, StateMachine, _offset, PICO_DEFAULT_LED_PIN );
        pio_sm_set_enabled(_pio, StateMachine, true);
    }

    // Setter function...
    void Set_Frequency(int _frequency){
        Freq = _frequency;                                                  // Copy parm to class var
        // Frequency scaled by 2000 as blink.pio requires this number of cycles to complete...
        float DAC_div = (float)clock_get_hz(clk_sys) /((float)_frequency*2000);
        pio_sm_set_clkdiv(pio, StateMachine, DAC_div);                      // Set the State Machine clock speed
    }
};

void SysInfo ( DAC DAC[], blink_forever LED_blinky) {
    // Print system and resource allocation details...
    sprintf(outStr,"\t|-----------------------------------------------------------|\n"
                   "\t| Resource allocation                                       |\n"
                   "\t|-----------------------------|-----------------------------|\n"
                   "\t| LED blinker                 |                             |\n"
                   "\t|-----------------------------|                             |\n"
                   "\t|   PIO:          %2d          |  Key:                       |\n"
                   "\t|   SM:           %2d          |   SM = State machine        |\n"
                   "\t|   GPIO:         %2d          |   BM = Bitmap               |\n"
                   "\t|   Frequency:    %2dHz        |                             |\n"
                   "\t|-----------------------------|-----------------------------|\n"
                   "\t| DAC Channel A               | DAC Channel B               |\n"
                   "\t|-----------------------------|-----------------------------|\n"
                   "\t| Frequency:     %3d          | Frequency:     %3d          |\n"
                   "\t| Phase:         %3d          | Phase:         %3d          |\n"
                   "\t| Duty cycle:    %3d          | Duty cycle:    %3d          |\n"
                   "\t| Divider:     %05d          | Divider:     %05d          |\n"
                   "\t|-----------------------------|-----------------------------|\n"
                   "\t| PIO:             %d          | PIO:             %d          |\n"
                   "\t| GPIO:          %d-%d          | GPIO:         %d-%d          |\n"
                   "\t| BM size:  %8d          | BM size:  %8d          |\n"
                   "\t| BM start: %x          | BM start: %x          |\n"
                   "\t| SM:              %d          | SM:              %d          |\n"
                   "\t| Wrap Bottom:    %2x          | Wrap Bottom:    %2x          |\n"
                   "\t| Wrap Top:       %2x          | Wrap Top:       %2x          |\n"
                   "\t| DMA ctrl:       %2d          | DMA ctrl:       %2d          |\n"
                   "\t| DMA data:       %2d          | DMA data:       %2d          |\n"
                   "\t|-----------------------------|-----------------------------|\n",
                   LED_blinky.pioNum,    LED_blinky.StateMachine, PICO_DEFAULT_LED_PIN, LED_blinky.Freq,
                   DAC[_A].Freq,                       DAC[_B].Freq,
                   DAC[_A].Phase,                      DAC[_B].Phase,
                   DAC[_A].DutyC,                      DAC[_B].DutyC,
              (int)DAC[_A].DAC_div,               (int)DAC[_B].DAC_div,
                   DAC[_A].PIOnum,                     DAC[_B].PIOnum,
                   DAC[_A].GPIO, DAC[_A].GPIO+7,       DAC[_B].GPIO, DAC[_B].GPIO+7,
                   BitMapSize,                         BitMapSize,
             (int)&DAC[_A].DAC_data[0],          (int)&DAC[_B].DAC_data[0],
                   DAC[_A].StateMachine,               DAC[_B].StateMachine,
                   DAC[_A].SM_WrapBot,                 DAC[_B].SM_WrapBot,
                   DAC[_A].SM_WrapTop,                 DAC[_B].SM_WrapTop, 
                   DAC[_A].ctrl_chan,                  DAC[_B].ctrl_chan,
                   DAC[_A].data_chan,                  DAC[_B].data_chan );
}

static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(Display_CS, 0);                                                      // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(Display_CS, 1);
    asm volatile("nop \n nop \n nop");
}

static void SPI_Display_Write(int _data) {
    uint8_t buff[2];
    buff[0] = _data / 256;                                                      // MSB data
    buff[1] = _data % 256;                                                      // LSB data
    cs_select();
    spi_write_blocking(SPI_PORT, buff, 2);
    cs_deselect();
}

static void getLine() {
    char *pPos = (char *)inStr ;                            // Pointer to start of Global input string
    while(1) {
        c = getchar();
        if (c == eof || c == '\n' || c == '\r') break ;     // Non blocking exit
        putchar(c);                                         // FullDuplex echo
        *pPos++ = c ;                                       // Bump pointer, store character
    }
    *pPos = '\0';                                           // Set string end mark
    return ;
}

int main() {
    stdio_init_all();

// Set SPI0 at 0.5MHz.
    spi_init(SPI_PORT, 500 * 1000);
    gpio_set_function(PIN_CLK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX, GPIO_FUNC_SPI);

// Chip select is active-low, so initialise to a driven-high state...
    gpio_init(Display_CS);
    gpio_set_dir(Display_CS, GPIO_OUT);
    gpio_put(Display_CS, 1);

// Initialise remaining SPI connections...
    gpio_set_dir(PIN_CLK, GPIO_OUT);
    gpio_set_dir(PIN_TX, GPIO_OUT);

    DAC DAC[2];                                                // Array to hold the two DAC channel objects

// Instantiate objects to control the various State Machines...
// Note: Both DAC channels need to be on the same PIO to achieve
//       Atomic restarts for accurate phase sync.
    DAC[_A].DAC_chan('A',pio1,0);                                     // First  DAC channel object in array - resistor network connected to GPIO0->8
    DAC[_B].DAC_chan('B',pio1,8);                                     // Second DAC channel object in array - resistor network connected to GPIO8->16
    blink_forever LED_blinky(pio0);                                   // Onboard LED blinky object

    strcpy(LastCmd,"?") ;                                             // Hitting return will give 'Help'

    SPI_Display_Write(0) ;                                            // Zero => SPI display

    LED_blinky.Set_Frequency(1);                                      // Flash LED at 1Hz- waiting for USB connection

    while (!stdio_usb_connected()) { sleep_ms(100); }                 // Wait for USB connection...

    LED_blinky.Set_Frequency(10);                                     // Flash LED at 10Hz - USB connected.
    SPI_Display_Write(DAC[_A].Freq) ;                                 // Frequency => SPI display

// Send start-up text to terminal...
    printf("==============================\n"                         // Version details to terminal    (optional)
           "WaveForm Generator Version 1.0\n"
           "==============================\n") ;

// Atomic Restart - starting all 4 DMA channels simultaneously ensures phase sync between both DAC channels
    dma_start_channel_mask(DAC_channel_mask);

    while(1) {
        ParmCnt=0, Parm[0]=0,  Parm[1]=0,  Parm[2]=0,  Parm[3]=0;                       // Reset all command line parameters
        printf(">") ;                                                                   // Command prompt

        getLine() ;

        // Zero length string = 'CR' pressed...
        if (strlen(inStr) == 0) { strcpy(inStr,LastCmd) ;                               // Repeat last command    
                                  printf("%s", inStr) ; }

        // One character commands...
        if (strlen(inStr) == 1) {
            if (inStr[0] == '?') sprintf(outStr,HelpText);                              // Help text
            if (inStr[0] == 'S') { DAC[_A].StatusString() ; DAC[_B].StatusString() ; }
            if (inStr[0] == 'R') SysInfo(DAC, LED_blinky);
        }

        // For all remaining commands, the first character selects DAC channel A or B...
        if (inStr[0] == 'A') { SelectedChan = 0b0001; }                                  // Channel A only
        if (inStr[0] == 'B') { SelectedChan = 0b0010; }                                  // Channel B only
        if (inStr[0] == 'C') { SelectedChan = 0b0011; }                                  // Channel A & B

        // ...and if we aren't bumping a value, there will be one or more parameters...
        if ((inStr[2] != '+') && (inStr[2] != '-')) {
            i = 2 ;                                                                      // Skip chars 0, 1 and 2
            while (i++ < strlen(inStr)-1 ) {                                             // Starts at char 3
                if ( inStr[i] == ',' ) { ParmCnt++ ; }                                   // Next parameter
                else                      { Parm[ParmCnt] *= 10;                         // Next digit. Bump the existing decimal digits
                                            Parm[ParmCnt] += inStr[i] - '0'; }           // Convert character to integer and add
            }
        }

        if (strlen(inStr) == 2) {
            if (inStr[1] == 'h') {                                                       // Set Hz
                if (SelectedChan & 0b01) DAC[_A].Set(_Range_,1) ;
                if (SelectedChan & 0b10) DAC[_B].Set(_Range_,1) ;
                dma_start_channel_mask(DAC_channel_mask);                                // Atomic restart both DAC channels
            }
            if (inStr[1] == 'k') {                                                       // Set KHz
                if (SelectedChan & 0b01) DAC[_A].Set(_Range_,1000) ;
                if (SelectedChan & 0b10) DAC[_B].Set(_Range_,1000) ;
                dma_start_channel_mask(DAC_channel_mask);                                // Atomic restart both DAC channels
            }
        }

        if ((inStr[1] == 'f') & (inStr[2] == 'r')) {                                    // Set Frequency
            if (inStr[3] == '+') {                                                      // Bump up and grab result for SPI display...
                if (SelectedChan & 0b01) result = DAC[_A].Bump(_Freq_,_Up) ;
                if (SelectedChan & 0b10) result = DAC[_B].Bump(_Freq_,_Up) ;
            } else if (inStr[3] == '-') {                                               // Bump down and grab result for SPI display...
                if (SelectedChan & 0b01) result = DAC[_A].Bump(_Freq_,_Down) ;
                if (SelectedChan & 0b10) result = DAC[_B].Bump(_Freq_,_Down) ;
            } else {                                                                    // Not a bump, so set the absolute value from Parm[0]...
                if (SelectedChan & 0b01) result = DAC[_A].Set(_Freq_,Parm[0]) ;
                if (SelectedChan & 0b10) result = DAC[_B].Set(_Freq_,Parm[0]) ;
                dma_start_channel_mask(DAC_channel_mask);                               // Atomic restart both DAC channels
            }
        }
        if ((inStr[1] == 'p') & (inStr[2] == 'h')) {                                    // Set Phase
            if (inStr[3] == '+') {                                                      // Bump up and grab result for SPI display...
                if (SelectedChan & 0b01) result = DAC[_A].Bump(_Phase_,_Up) ;
                if (SelectedChan & 0b10) result = DAC[_B].Bump(_Phase_,_Up) ;
            } else if (inStr[3] == '-') {                                               // Bump down and grab result for SPI display...
                if (SelectedChan & 0b01) result = DAC[_A].Bump(_Phase_,_Down) ;
                if (SelectedChan & 0b10) result = DAC[_B].Bump(_Phase_,_Down) ;
            } else {                                                                    // Not a bump, so set the absolute value from Parm[0]...
                if (SelectedChan & 0b01) result = DAC[_A].Set(_Phase_,Parm[0]) ;
                if (SelectedChan & 0b10) result = DAC[_B].Set(_Phase_,Parm[0]) ;
                dma_start_channel_mask(DAC_channel_mask);                               // Atomic restart both DAC channels
            }
        }

        if ((inStr[1] == 'd') & (inStr[2] == 'u')) {                                    // Set Duty cycle
            if (inStr[3] == '+') {                                                      // Bump up and grab result for SPI display...
                if (SelectedChan & 0b01) result = DAC[_A].Bump(_Duty_,_Up) ;
                if (SelectedChan & 0b10) result = DAC[_B].Bump(_Duty_,_Up) ;
            } else if (inStr[3] == '-') {                                               // Bump down and grab result for SPI display...
                if (SelectedChan & 0b01) result = DAC[_A].Bump(_Duty_,_Down) ;
                if (SelectedChan & 0b10) result = DAC[_B].Bump(_Duty_,_Down) ;
            } else {                                                                    // Not a bump, so set the absolute value from Parm[0]...
                if ( Parm[0] > 100 ) Parm[0] = 100;                                     // Hard limit @ 100%
                if (SelectedChan & 0b01) DAC[_A].Set(_Duty_,Parm[0]) ;
                if (SelectedChan & 0b10) DAC[_B].Set(_Duty_,Parm[0]) ;
            }
        }

        // Next two characters select the command...
        if (strlen(inStr) == 3) {                                                       // TBD - is this needed ???
            if ((inStr[1] == 's') & (inStr[2] == 'i')) {                                // Set Sine
                if (SelectedChan & 0b01) DAC[_A].Set(_Funct_,_Sine_) ;
                if (SelectedChan & 0b10) DAC[_B].Set(_Funct_,_Sine_) ;
            } else if ((inStr[1] == 's') & (inStr[2] == 'q')) {                         // Set Square
                if (SelectedChan & 0b01) DAC[_A].Set(_Funct_,_Square_) ;
                if (SelectedChan & 0b10) DAC[_B].Set(_Funct_,_Square_) ;
            } else if ((inStr[1] == 't') & (inStr[2] == 'r')) {                         // Set Triangle
                if (SelectedChan & 0b01) DAC[_A].Set(_Funct_,_Triangle_) ;
                if (SelectedChan & 0b10) DAC[_B].Set(_Funct_,_Triangle_) ;
            } 
        }

        if ((inStr[1] == 's') & (inStr[2] == 'w')) {                                    // Sweep
        // Parm[0]=Low frequency, Parm[1]=High frequency, Parm[2]=Scan speed, Parm[3]=Low/High pause
            i = Parm[0];
            for (;;) {
                outStr[0] = '\0' ;                                                          // Reset the string variable
                if (SelectedChan & 0b01) result = DAC[_A].Set(_Freq_,i) ;                   // Set frequency, display status
                if (SelectedChan & 0b10) result = DAC[_B].Set(_Freq_,i) ;                   // Set frequency, display status
                dma_start_channel_mask(DAC_channel_mask);                                   // Atomic restart all 4 DMA channels...
                printf(outStr) ;                                                            // Update terminal
                SPI_Display_Write(i);                                                       // Update SPI display
                if (i==Parm[0]) { dirn = 1;
                                  sleep_ms(Parm[3]); }
                if (i>=Parm[1]) { dirn =-1; 
                                  sleep_ms(Parm[3]); }
                dma_start_channel_mask(DAC_channel_mask);                                   // Atomic restart both DAC channels
                i = i + dirn;
                c = getchar_timeout_us (0);                                                 // Non-blocking char input
                if ((c>=32) & (c<=126)) { break; }                                          // exit on keypress
                sleep_ms(Parm[2]);                                                          // Speed of scan
            }
        }

        if (strlen(outStr) == 0) strcpy(outStr,"\t?\n") ;                               // Unknown command
        printf(outStr) ;                                                                // Update terminal
        outStr[0] = '\0' ;                                                              // Clear (reset) the string variable
        SPI_Display_Write(result) ;                                                     // Update SPI display
        strcpy(LastCmd, inStr) ;                                                        // Preserve last command
    }
    return 0;
}
