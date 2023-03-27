#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include <math.h>
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "blink.pio.h"
#include "FastDAC.pio.h"
#include "SlowDAC.pio.h"

/////////////////////////////
// Define GPIO connections...
/////////////////////////////

// Note: The SPI Port only works through specific pins, so this port is defined first.
// SPI Port connections...                          // ┌──────────┬───────────────┬─────────────┐──────────────┐
                                                    // │ PGA2040  │ Connection    │ MCP41010    │ Nixie module │
                                                    // ├──────────┼───────────────┼─────────────┤──────────────┤
#define PIN_RX          16                          // │ GPIO 16  │ RX/spi1_rx    │             │      -       │
//#define PIN_CS          17                        // │ GPIO 17  │ CS/spi1_cs    │             │              │ can this be re-defined ?
#define PIN_CLK         18                          // │ GPIO 18  │ CLK/spi1_clk  │             │  SCK (blue)  │
#define PIN_TX          19                          // │ GPIO 19  │ TX/spi1_tx    │             │  SDI (green) │
#define Nixie_CS        21                          // │ GPIO 21  │ Chip select   │             │  SS1 (white) │
                                                    // └──────────┴───────────────┴─────────────┘──────────────┘
#define SPI_PORT        spi0                        // These SPI connections require the use of RP2040 SPI port 0

#define _A              0                           // DAC channel alias
#define _B              1
#define LED             20                          // GPIO connected to LED
#define BitMapSize      256                         // Match X to Y resolution
//#define BitMapSize      360                       // won't work - DMA needs to operate as a power of 2
#define Slow            0
#define Fast            1
#define _Sine_          0                           // Permited values for variable WaveForm_Type
#define _Square_        1
#define _Triangle_      2
#define _GPIO_          0
#define _PIO_           1
#define _SM_fast_       2
#define _SM_slow_       3
#define _SM_code_fast_  4
#define _SM_code_slow_  5
#define _DMA_ctrl_fast_ 6
#define _DMA_ctrl_slow_ 7
#define _DMA_data_fast_ 8
#define _DMA_data_slow_ 9
#define _Funct_        10
#define _Phase_        11
#define _Freq_         12
#define _Range_        13
#define _DutyC_        14

// TBD - these should probably go in the object.
unsigned short DAC_data_A[BitMapSize] __attribute__ ((aligned(2048))) ;     // Align DAC data
unsigned short DAC_data_B[BitMapSize] __attribute__ ((aligned(2048))) ;     // Align DAC data

unsigned short DAC_channel_mask = 0 ;                                       // Binary mask to simultaneously start all DMA channels
const uint32_t transfer_count = BitMapSize ;                                // Number of DMA transfers per event
int Value, tmp, WaveForm_Type;
const uint startLineLength = 8;                                             // the linebuffer will automatically grow for longer lines
const char eof = 255;                                                       // EOF in stdio.h -is -1, but getchar returns int 255 to avoid blocking
const char *HelpText = 
"\tUsage...\n"
"\t  ?          - Usage\n"
"\t  S          - Status\n"
"\t  I          - System info\n"
"\t  <A/B>fnnn  - Frequency                  ( 0->999 )\n"
"\t  <A/B>h     - Frequency multiplier  Hz\n"
"\t  <A/B>k     - Frequency multiplier KHz\n"
"\t  <A/B>snnn  - Sine wave + harmonic       ( 0->9 )\n"
"\t  <A/B>qnnn  - Square wave + duty cycle   ( 0->100%% )\n"
"\t  <A/B>tnnn  - Triangle wave + duty cycle ( 0->100%% )\n"
"\t  p<A/B>nnn  - Phase                      ( 0->360 degrees )\n"
"\t  <A/B>      - DAC channel A or B\n"
"\t        nnn  - Three digit numeric value\n";

class DACchannel {
    uint Funct, Phase, Freq, Range, DutyC;
    PIO pio;                                                    // Class wide var to share value with setter function
    uint StateMachine[2] ;                                      // Fast and slow State Machines
    unsigned short * DAC_RAM ;                                  // Pointer to RAM data (selects DAC A or B)
    uint DAC_GPIO, _pioNum, SM_fast, SM_slow, SM_code_fast ;    // Variabes used by the getter function...
    uint SM_code_slow, ctrl_chan_fast, ctrl_chan_slow ;
    uint data_chan_fast, data_chan_slow ;

public:
    void SetFunct(int _value);                                  // Setter functions
    void SetPhase(int _value);
    void SetRange(int _value);
    void SetFreq (int _value);
    void SetDutyC(int _value);
    void DACclock(int _frequency);

    int Get_Resource(int _index);                               // Getter function

public:
    // Constructor
    // The PIO clock dividers are 16-bit integer, 8-bit fractional, with first-order delta-sigma for the fractional divider.
    // The clock divisor can vary between 1 and 65536, in increments of 1/256.
    // If DAC_div exceeds 2^16 (65,536), the registers wrap around, and the State Machine clock will be incorrect.
    // A slow version of the DAC State Machine is used for frequencies below 17Hz, allowing the value of DAC_div to
    // be kept within range.
        void NewDMAtoDAC_channel(PIO _pio) {
            pio = _pio;                                                             // transfer parameter to class wide var
            _pioNum = pio_get_index(_pio);
            _pioNum == 0 ? DAC_GPIO = 0 : DAC_GPIO = 8;                             // Select GPIO's   for DAC
            _pioNum == 0 ? DAC_RAM = DAC_data_A : DAC_RAM = DAC_data_B;             // Select data RAM for DAC
            StateMachine[Fast] = Single_DMA_FIFO_SM_GPIO_DAC(_pio,Fast,DAC_GPIO);   // Create the Fast DAC channel (frequencies: 17Hz to 999KHz)
            StateMachine[Slow] = Single_DMA_FIFO_SM_GPIO_DAC(_pio,Slow,DAC_GPIO);   // Create the Slow DAC channel (frequencies: 0Hz to 16Hz)
        };

public:
    int Single_DMA_FIFO_SM_GPIO_DAC(PIO _pio, int _speed, uint _startpin) {
    // Create a DMA channel and its associated State Machine.
    // DMA => FIFO => State Machine => GPIO pins => DAC
        uint _pioNum = pio_get_index(_pio);                                     // Get user friendly index number.
        int _offset;
        uint _StateMachine = pio_claim_unused_sm(_pio, true);                    // Find a free state machine on the specified PIO - error if there are none.
        uint ctrl_chan = dma_claim_unused_channel(true);                        // Find 2 x free DMA channels for the DAC (12 available)
        uint data_chan = dma_claim_unused_channel(true);

        if (_speed == 1) {
        // Configure the state machine to run the FastDAC program...
            SM_fast = _StateMachine;
            _offset = pio_add_program(_pio, &pio_FastDAC_program);
            SM_code_fast = _offset;
            pio_FastDAC_program_init(_pio, _StateMachine, _offset, _startpin);
            ctrl_chan_fast = ctrl_chan ;                                        // Make details available to getter functions
            data_chan_fast = data_chan ;
        } else {
        // Configure the state machine to run the SlowDAC program...
            SM_slow = _StateMachine;
            _offset = pio_add_program(_pio, &pio_SlowDAC_program);              // Use helper function included in the .pio file.
            SM_code_slow = _offset;
            pio_SlowDAC_program_init(_pio, _StateMachine, _offset, _startpin);
            ctrl_chan_slow = ctrl_chan ;                                        // Make details available to getter functions
            data_chan_slow = data_chan ;
        }

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
        channel_config_set_dreq(&fc, pio_get_dreq(_pio, _StateMachine, true));  // Transfer when PIO SM TX FIFO has space
        channel_config_set_chain_to(&fc, ctrl_chan);                            // chain to the controller DMA channel
        channel_config_set_ring(&fc, false, 9);                                 // 8 bit DAC 1<<9 byte boundary on read ptr. This is why we needed alignment!
        dma_channel_configure(
            data_chan,                                                          // Channel to be configured
            &fc,                                                                // The configuration we just created
            &_pio->txf[_StateMachine],                                          // Write to FIFO
            DAC_RAM,                                                            // The initial read address (AT NATURAL ALIGNMENT POINT)
            BitMapSize,                                                         // Number of transfers; in this case each is 2 byte.
            false                                                               // Don't start immediately. All 4 control channels need to start simultaneously
                                                                                // to ensure the correct phase shift is applied.
        );
        // Note: All DMA channels are left running permanently. 
        //       The active channel is selected by enabling/disabling the associated State Machine.
        DAC_channel_mask += (1u << ctrl_chan) ;                                 // Save details of DMA control channel to global variable

        return(_StateMachine);
    }
};

    void DACchannel::SetFunct(int _value) { Funct = _value; }                   // Function    (Sine/Triangl/Square)
    void DACchannel::SetPhase(int _value) { Phase = _value; }                   // Phase shift (0->360 degrees)
    void DACchannel::SetDutyC(int _value) { DutyC = _value; }                   // Duty cycle  (0->100%)
    void DACchannel::SetRange(int _value) { Range = _value;                     // Range       (Hz/KHz)
                                            DACclock(Freq * Range); }           // Update State MAchine run speed
    void DACchannel::SetFreq (int _value) { Freq  = _value;                     // Frequency   (numeric)
                                            DACclock(Freq * Range); }           // Update State MAchine run speed
    
    void DACchannel::DACclock(int _frequency){
        // If DAC_div exceeds 2^16 (65,536), the registers wrap around, and the State Machine clock will be incorrect.
        // A slow version of the DAC State Machine is used for frequencies below 17Hz, allowing the value of DAC_div to
        // be kept within range.
        float DAC_freq = _frequency * BitMapSize;                               // Target frequency...
        float DAC_div = 2 * (float)clock_get_hz(clk_sys) / DAC_freq;            // ...calculate the PIO clock divider required for the given Target frequency
        float Fout = 2 * (float)clock_get_hz(clk_sys) / (BitMapSize * DAC_div); // Actual output frequency
         if (_frequency >= 34) {                                                // Fast DAC ( Frequency range from 34Hz to 999Khz )
            pio_sm_set_clkdiv(pio, StateMachine[Fast], DAC_div);                // Set the State Machine clock speed
            pio_sm_set_enabled(pio, StateMachine[Fast], true);                  // Fast State Machine active
            pio_sm_set_enabled(pio, StateMachine[Slow], false);                 // Slow State Machine inactive
        } else {                                                                // Slow DAC ( 1Hz=>16Hz )
            DAC_div = DAC_div / 64;                                             // Adjust DAC_div to keep within useable range
            DAC_freq = DAC_freq * 64;
            pio_sm_set_clkdiv(pio, StateMachine[Slow], DAC_div);                // Set the State Machine clock speed
            pio_sm_set_enabled(pio, StateMachine[Fast], false);                 // Fast State Machine inactive
            pio_sm_set_enabled(pio, StateMachine[Slow], true);                  // Slow State Machine active
        }
    }

    int DACchannel::Get_Resource(int _index){
        int result;
        switch (_index) {
            case _GPIO_:          result = DAC_GPIO;       break;
            case _PIO_:           result = _pioNum;        break;
            case _SM_fast_:       result = SM_fast;        break;
            case _SM_slow_:       result = SM_slow;        break;
            case _SM_code_fast_ : result = SM_code_fast;   break;
            case _SM_code_slow_ : result = SM_code_slow;   break;
            case _DMA_ctrl_fast_: result = ctrl_chan_fast; break;
            case _DMA_ctrl_slow_: result = ctrl_chan_slow; break;
            case _DMA_data_fast_: result = data_chan_fast; break;
            case _DMA_data_slow_: result = data_chan_slow; break;
            case _Funct_:         result = Funct;          break;
            case _Phase_:         result = Phase;          break;
            case _Freq_:          result = Freq;           break;
            case _Range_:         result = Range;          break;
            case _DutyC_:         result = DutyC;          break;
        }
    return (result);
    }

class blink_forever {                                                      // Class to initialise a state machine to blink a GPIO pin
PIO pio ;                                                                   // Class wide variables to share value with setter function
uint StateMachine, _offset ;
public:
    blink_forever(PIO _pio ) {
        pio = _pio;                                                        // transfer parameter to class wide var
        StateMachine = pio_claim_unused_sm(_pio, true);                    // Find a free state machine on the specified PIO - error if there are none.
        _offset = pio_add_program(_pio, &pio_blink_program);
        blink_program_init(_pio, StateMachine, _offset, LED );
        pio_sm_set_enabled(_pio, StateMachine, true);
    }

    // Setter functions...
    void Set_Frequency(int _frequency){
    // Frequency scaled by 2000 as blink.pio requires this number of cycles to complete...
        float DAC_div = (float)clock_get_hz(clk_sys) /((float)_frequency*2000);
        pio_sm_set_clkdiv(pio, StateMachine, DAC_div);                      // Set the State Machine clock speed
    }
};

void WaveForm_Update(int _DAC_select, int _WaveForm_Type, int _WaveForm_Value, int _Phase) {
//    int i,h_index, v_offset = BitMapSize/2 - 1;                                                            // Shift sine waves up above X axis
    int i,j, v_offset = 256/2 - 1;                                                            // Shift sine waves up above X axis
    const float _2Pi = 6.283;                                                                              // 2*Pi
    float a,b,x1,x2,g1,g2;
    unsigned short *DAC_data;                                                                              // Pointer to either DAC A or B data area

// Pointer to selected DAC data area...
    _DAC_select == 0 ? DAC_data = DAC_data_A : DAC_data = DAC_data_B;
// Scale the phase shift to match data size...    
    _Phase = _Phase * BitMapSize / 360 ;       // Input  range: 0 -> 360 (degrees)
                                               // Output range: 0 -> 255 (bytes)
    switch (_WaveForm_Type) {
        case _Sine_:
            _WaveForm_Value = _WaveForm_Value % 10;                                                        // Sine value cycles after 7
            for (i=0; i<BitMapSize; i++) {
// Add the phase offset and wrap data beyond buffer end back to the buffer start...
                j = ( i + _Phase ) % BitMapSize;                                                            // Horizontal index
                a = v_offset * sin((float)_2Pi*i / (float)BitMapSize);                                      // Fundamental frequency...
                if (_WaveForm_Value >= 1) { a += v_offset/3  * sin((float)_2Pi*3*i  / (float)BitMapSize); } // Add  3rd harmonic
                if (_WaveForm_Value >= 2) { a += v_offset/5  * sin((float)_2Pi*5*i  / (float)BitMapSize); } // Add  5th harmonic
                if (_WaveForm_Value >= 3) { a += v_offset/7  * sin((float)_2Pi*7*i  / (float)BitMapSize); } // Add  7th harmonic
                if (_WaveForm_Value >= 4) { a += v_offset/9  * sin((float)_2Pi*9*i  / (float)BitMapSize); } // Add  9th harmonic
                if (_WaveForm_Value >= 5) { a += v_offset/11 * sin((float)_2Pi*11*i / (float)BitMapSize); } // Add 11th harmonic
                if (_WaveForm_Value >= 6) { a += v_offset/13 * sin((float)_2Pi*13*i / (float)BitMapSize); } // Add 13th harmonic
                if (_WaveForm_Value >= 7) { a += v_offset/15 * sin((float)_2Pi*15*i / (float)BitMapSize); } // Add 15th harmonic
                if (_WaveForm_Value >= 8) { a += v_offset/17 * sin((float)_2Pi*17*i / (float)BitMapSize); } // Add 17th harmonic
                if (_WaveForm_Value >= 9) { a += v_offset/19 * sin((float)_2Pi*19*i / (float)BitMapSize); } // Add 19th harmonic
                DAC_data[j] = (int)(a)+v_offset;                                                      // Sum all harmonics and add vertical offset
            }
            break;
        case _Square_: 
            b = _WaveForm_Value * BitMapSize / 100;                                                         // Convert % to value
            for (i=0; i<BitMapSize; i++) {
                if (b <= i) { DAC_data[i] = 0;   }                                                          // First section low
                else        { DAC_data[i] = 255; }                                                          // Second section high
            }
            break;
        case _Triangle_: 
            x1 = (_WaveForm_Value * BitMapSize / 100) -1;                                                   // Number of data points to peak
            x2 = BitMapSize - x1;                                                                           // Number of data points after peak
            g1 = (BitMapSize - 1) / x1;                                                                     // Rising gradient (Max val = BitMapSize -1)
            g2 = (BitMapSize - 1) / x2;                                                                     // Falling gradient (Max val = BitMapSize -1)
            for (i=0; i<BitMapSize; i++) {
                if (i <= x1) { DAC_data[i] = i * g1; }                                                      // Rising  section of waveform...
                if (i > x1)  { DAC_data[i] = (BitMapSize - 1) - ((i - x1) * g2); }                          // Falling section of waveform
            }
            break;
    }
}

void ChanInfo ( DACchannel DACchannel[], int _chanNum) {
// Print current channel parameters to the console...
    char Chan, WaveStr[9], MultStr[4];
    int value = DACchannel[_chanNum].Get_Resource(_Funct_);
    switch ( value ) {
        case _Sine_:     strcpy(WaveStr, "Sine");     break;
        case _Triangle_: strcpy(WaveStr, "Triangle"); break;
        case _Square_:   strcpy(WaveStr,"Square");
    }
    _chanNum == 0 ? Chan = 'A' : Chan = 'B';
    DACchannel[_chanNum].Get_Resource(_Range_) == 1 ? strcpy(MultStr,"Hz ") : strcpy(MultStr,"KHz");
    printf("\tChannel %c: Freq:%03d%s Phase:%03d  Wave:%s\n", Chan, DACchannel[_chanNum].Get_Resource(_Freq_),
        MultStr, DACchannel[_chanNum].Get_Resource(_Phase_), WaveStr);
}

void SysInfo ( DACchannel DACchannel[]) {
// Print system and resource allocation details...
    int a,b,c,d ;
    printf("\n|-----------------------------------------------------------|\n");
    printf("| Waveform Generator Ver: 0.0.1       Date: 21/03/2013      |\n");
    printf("|-----------------------------|-----------------------------|\n");
    printf("| Channel A                   | Channel B                   |\n");
    printf("|-----------------------------|-----------------------------|\n");
    a = DACchannel[_A].Get_Resource(_PIO_);
    b = DACchannel[_B].Get_Resource(_PIO_);
    printf("| PIO:             %d          | PIO:             %d          |\n",a,b);
    a = DACchannel[_A].Get_Resource(_GPIO_);
    b = DACchannel[_B].Get_Resource(_GPIO_);
    printf("| GPIO:          %d-%d          | GPIO:         %d-%d          |\n",a,a+7,b,b+7);
    printf("| BM size:  %8d          | BM size:  %8d          |\n", BitMapSize,  BitMapSize);
    printf("| BM start: %x          | BM start: %x          |\n", &DAC_data_A[0],  &DAC_data_B[0]);
    printf("|--------------|--------------|--------------|--------------|\n");
    printf("| Fast DAC     | Slow DAC     | Fast DAC     |  Slow DAC    |\n");
    printf("|--------------|--------------|--------------|--------------|\n");
    a = DACchannel[_A].Get_Resource(_SM_fast_);
    b = DACchannel[_A].Get_Resource(_SM_slow_);
    c = DACchannel[_B].Get_Resource(_SM_fast_);
    d = DACchannel[_B].Get_Resource(_SM_slow_);
    printf("| SM:        %d | SM:        %d | SM:        %d | SM:        %d |\n",a,b,c,d);
    a = DACchannel[_A].Get_Resource(_SM_code_fast_);
    b = DACchannel[_A].Get_Resource(_SM_code_slow_);
    c = DACchannel[_B].Get_Resource(_SM_code_fast_);
    d = DACchannel[_B].Get_Resource(_SM_code_slow_);
    printf("| SM code:  %2d | SM code:  %2d | SM code:  %2d | SM code:  %2d |\n",a,b,c,d);
    a = DACchannel[_A].Get_Resource(_DMA_ctrl_fast_);                         // Get DMA control channel numbers
    b = DACchannel[_A].Get_Resource(_DMA_ctrl_slow_);
    c = DACchannel[_B].Get_Resource(_DMA_ctrl_fast_);
    d = DACchannel[_B].Get_Resource(_DMA_ctrl_slow_);
    printf("| DMA ctrl: %2d | DMA ctrl: %2d | DMA ctrl: %2d | DMA ctrl: %2d |\n",a,b,c,d);
    a = DACchannel[_A].Get_Resource(_DMA_data_fast_);                         // Get DMA control channel numbers
    b = DACchannel[_A].Get_Resource(_DMA_data_slow_);
    c = DACchannel[_B].Get_Resource(_DMA_data_fast_);
    d = DACchannel[_B].Get_Resource(_DMA_data_slow_);
    printf("| DMA data: %2d | DMA data: %2d | DMA data: %2d | DMA data: %2d |\n",a,b,c,d); 
    printf("|--------------|--------------|--------------|--------------|\n");
}

static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(Nixie_CS, 0);  // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(Nixie_CS, 1);
    asm volatile("nop \n nop \n nop");
}

static void SPI_Nixie_Write(int _data) {
    uint8_t buff[2];
    buff[0] = _data / 256;                                                      // MSB data
    buff[1] = _data % 256;                                                      // LSB data
    cs_select();
    spi_write_blocking(SPI_PORT, buff, 2);
    cs_deselect();
}

static char * getLine(bool fullDuplex = false, char lineBreak = '\n') {
/*
 *  read a line of any  length from stdio (grows)
 *
 *  @param fullDuplex input will echo on entry (terminal mode) when false
 *  @param linebreak defaults to "\n", but "\r" may be needed for terminals
 *  @return entered line on heap - don't forget calling free() to get memory back
 */
    // th line buffer
    // will allocated by pico_malloc module if <cstdlib> gets included
    char * pStart = (char*)malloc(startLineLength); 
    char * pPos = pStart;  // next character position
    size_t maxLen = startLineLength; // current max buffer size
    size_t len = maxLen; // current max length
    int c;

    if(!pStart) {
        return NULL; // out of memory or dysfunctional heap
    }

    while(1) {
        c = getchar(); // expect next character entry
        if(c == eof || c == lineBreak) {
            break;     // non blocking exit
        }
        if (fullDuplex) {
            putchar(c); // echo for fullDuplex terminals
        }
        if(--len == 0) { // allow larger buffer
            len = maxLen;
            // double the current line buffer size
            char *pNew  = (char*)realloc(pStart, maxLen *= 2);
            if(!pNew) {
                free(pStart);
                return NULL; // out of memory abort
            }
            // fix pointer for new buffer
            pPos = pNew + (pPos - pStart);
            pStart = pNew;
        }
        // stop reading if lineBreak character entered 
        if((*pPos++ = c) == lineBreak) {
            break;
        }
    }
    *pPos = '\0';   // set string end mark
    return pStart;
}

int main() {
    stdio_init_all();

// Set SPI0 at 0.5MHz.
    spi_init(SPI_PORT, 500 * 1000);
    gpio_set_function(PIN_CLK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX, GPIO_FUNC_SPI);

// Chip select is active-low, so initialise to a driven-high state...
    gpio_init(Nixie_CS);
    gpio_set_dir(Nixie_CS, GPIO_OUT);
    gpio_put(Nixie_CS, 1);

// Initialise remaining SPI connections...
    gpio_set_dir(PIN_CLK, GPIO_OUT);
    gpio_set_dir(PIN_TX, GPIO_OUT);

    DACchannel DACchannel[2];                                                // Array to hold the two DAC channel objects

// Set up the objects controlling the various State Machines...
// Note: I may need to move both DMA to DAC channels onto the same PIO to acheive accurate phase sync. But for now,
//       I have just distributed the load across the two PIO's
    DACchannel[_A].NewDMAtoDAC_channel(pio0);                                   // Create the first DAC channel object in the array
    DACchannel[_B].NewDMAtoDAC_channel(pio1);                                   // Create the second DAC channel object in the array
    blink_forever my_blinker(pio0);                                             // Onboard LED blinky object

// Set LED to rapid flash indicates waiting for USB connection...
    my_blinker.Set_Frequency(1);                                                // 1Hz

// Wait for USB connection...
    while (!stdio_usb_connected()) { sleep_ms(100); }

// USB connection established, set LED to regular flash...
    my_blinker.Set_Frequency(10);                                                // 10Hz

    SysInfo(DACchannel); ;                                                      // Show configuration (optional)
//  printf(HelpText);                                                           // Show instructions  (optional)

// Set default run time settings...
    DACchannel[_A].SetFreq(100),     DACchannel[_B].SetFreq(100) ;              // 100
    DACchannel[_A].SetRange(1),      DACchannel[_B].SetRange(1) ;               // Hz
    DACchannel[_A].SetPhase(0),      DACchannel[_B].SetPhase(180) ;             // 180 phase diff
    DACchannel[_A].SetFunct(_Sine_), DACchannel[_B].SetFunct(_Sine_) ;          // Sine wave, no harmonics
    DACchannel[_A].SetDutyC(50),     DACchannel[_B].SetDutyC(50);               // 50% Duty cycle

    WaveForm_Update(_A, _Sine_, DACchannel[_A].Get_Resource(_DutyC_), DACchannel[_A].Get_Resource(_Phase_));
    WaveForm_Update(_B, _Sine_, DACchannel[_B].Get_Resource(_DutyC_), DACchannel[_B].Get_Resource(_Phase_));

    SPI_Nixie_Write(DACchannel[_A].Get_Resource(_Freq_));                         // Update the Nixie display

// Start all 4 DMA channels simultaneously - this ensures phase sync across all State Machines...
    dma_start_channel_mask(DAC_channel_mask);

    while(1) {
        char *inString = getLine(true, '\r') ;
        tmp = strlen(inString) ;
        int SelectedChan ;

        if ((tmp<=0) or (tmp>5)) {
            printf("  Syntax error\n");
        } else {
            if      ( inString[0] == '?' ) { printf(HelpText); }                     // Help text
            else if ( inString[0] == 'S' ) { ChanInfo(DACchannel, _A);                            // Status info
                                             ChanInfo(DACchannel, _B);      }
            else if ( inString[0] == 'I' ) { SysInfo(DACchannel); }
            else {
                // Select DAC channel A or B...
                inString[0] == 'A' ? SelectedChan = 0 : SelectedChan = 1;
                // Find numeric value, based on number of parameters passed. This ensures leading zeros are be ignored...
                // 1 digit...
                if ( tmp == 3 ) { Value =   inString[2] - '0'; }
                // 2 digits...
                if ( tmp == 4 ) { Value = ((inString[2]-'0') * 10) +   (inString[3]-'0'); }
                // 3 digits...
                if ( tmp == 5 ) { Value = ((inString[2]-'0') * 100) + ((inString[3]-'0') * 10) + (inString[4]-'0'); }

                switch ( inString[1] ) {
                    case 's':                                               // Sine wave
                        DACchannel[SelectedChan].SetFunct(_Sine_);
                        DACchannel[SelectedChan].SetDutyC(Value);
                        WaveForm_Update(SelectedChan, _Sine_, DACchannel[SelectedChan].Get_Resource(_DutyC_),
                            DACchannel[SelectedChan].Get_Resource(_Phase_));
                        break;
                    case 't':                                               // Triangle wave
                        if ( Value > 100 ) { Value = 100; }                 // Hard limit @ 100%
                        DACchannel[SelectedChan].SetFunct(_Triangle_);
                        DACchannel[SelectedChan].SetDutyC(Value);
                        WaveForm_Update(SelectedChan, _Triangle_, DACchannel[SelectedChan].Get_Resource(_DutyC_),
                            DACchannel[SelectedChan].Get_Resource(_Phase_));
                        break;
                    case 'q':
                        if ( Value > 100 ) { Value = 100; }                 // Hard limit @ 100%
                        DACchannel[SelectedChan].SetFunct(_Square_);
                        DACchannel[SelectedChan].SetDutyC(Value);
                        WaveForm_Update(SelectedChan, _Square_, DACchannel[SelectedChan].Get_Resource(_DutyC_),
                            DACchannel[SelectedChan].Get_Resource(_Phase_));
                        break;
                    case 'h':                                               // Set Hz
                        DACchannel[SelectedChan].SetRange(1);
                        break;
                    case 'k':                                               // Set KHz
                        DACchannel[SelectedChan].SetRange(1000);
                        break;
                    case 'f':
                        // Stop the DMA data transfer...
                        hw_clear_bits(&dma_hw->ch[1].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[3].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[5].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[7].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);

                        DACchannel[SelectedChan].SetFreq(Value);

                        // Reset the DMA channel pointers to the start of the bitmap data...
                        //   ( this forces a phase lock between channel A and channel B )
                        dma_hw->ch[1].al1_read_addr = (long unsigned int)&DAC_data_A[0];
                        dma_hw->ch[3].al1_read_addr = (long unsigned int)&DAC_data_A[0];
                        dma_hw->ch[5].al1_read_addr = (long unsigned int)&DAC_data_B[0];
                        dma_hw->ch[7].al1_read_addr = (long unsigned int)&DAC_data_B[0];
                    
                        // Restart the DMA data channels
                        hw_set_bits(&dma_hw->ch[1].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[3].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[5].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[7].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);

                        // Would be much tighter if I could...
                        // Start all 4 DMA channels simultaneously - this ensures phase sync across all State Machines...
                        // dma_start_channel_mask(DAC_channel_mask);
                        break;
                    case 'p':
                        hw_clear_bits(&dma_hw->ch[0].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[1].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[2].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[3].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[4].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[5].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[6].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_clear_bits(&dma_hw->ch[7].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);

                        dma_hw->abort = (1 << 0) | (1 << 1);
                        dma_hw->abort = (1 << 2) | (1 << 3);
                        dma_hw->abort = (1 << 4) | (1 << 5);
                        dma_hw->abort = (1 << 6) | (1 << 7);

                        // dma_hw->abort = DAC_channel_mask;

                        DACchannel[SelectedChan].SetPhase(Value);

                        WaveForm_Update(SelectedChan, DACchannel[SelectedChan].Get_Resource(_Funct_),
                            DACchannel[SelectedChan].Get_Resource(_DutyC_), DACchannel[SelectedChan].Get_Resource(_Phase_));

                        hw_set_bits(&dma_hw->ch[0].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[1].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[2].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[3].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[4].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[5].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[6].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        hw_set_bits(&dma_hw->ch[7].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
                        
                        // Start all 4 DMA channels simultaneously - this ensures phase sync across all State Machines...
                        dma_start_channel_mask(DAC_channel_mask);

                        break;
                    default:
                        printf("\tUnknown command\n");
                }
                ChanInfo(DACchannel, SelectedChan);                             // Update the terminal
                SPI_Nixie_Write(Value);                             // Update the Nixie display
            }
        }
        free(inString);                        // free buffer
    }
    return 0;
}
