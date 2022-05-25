#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "rotary_encoder.pio.h"
#include "blink.pio.h"
#include "FastDAC.pio.h"
#include "SlowDAC.pio.h"

#define sine_table_size 256         // Number of samples per period in sine table
#define SW_2way_1       13          // GPIO connection
#define SW_3way_1       14          // GPIO connection
#define SW_3way_2       15          // GPIO connection

// Global variables...
int SW_2way, Last_SW_2way, SW_3way, Last_SW_3way, ScanCtr, NixieVal, Frequency;
int UpdateReq;                                      // Flag from Rotary Encoder to main loop indicating a value has changed

int DAC[5]              = { 2, 3, 4, 5, 6 };        // DAC ports                                - DAC0=>2  DAC4=>6
int NixieCathodes[4]    = { 18, 19, 20, 21 };       // GPIO ports connecting to Nixie Cathodes  - Data0=>18     Data3=>21
int NixieAnodes[3]      = { 22, 26, 27 };           // GPIO ports connecting to Nixie Anodes    - Anode0=>22    Anode2=>27
int EncoderPorts[2]     = { 16, 17 };               // GPIO ports connecting to Rotary Encoder  - 16=>Clock     17=>Data
int NixieBuffer[3]      = { 6, 7, 8 };              // Values to be displayed on Nixie tubes    - Tube0=>1's
                                                    //                                          - Tube1=>10's
                                                    //                                          - Tube2=>100's
int raw_sin[sine_table_size] ;
unsigned short DAC_data[sine_table_size] __attribute__ ((aligned(2048))) ;          // Align DAC data
const uint32_t transfer_count = sine_table_size ;                                   // Number of DMA transfers per event

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq);

class RotaryEncoder {                                                               // class to initialise a state machine to read 
public:                                                                             //    the rotation of the rotary encoder
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

        printf("PIO:0 SM:%d - Rotary encoder' @ %dHz\n\n", sm, freq);
    }

    void set_Frequency(int _Frequency) { Frequency = _Frequency; }
    void set_WaveForm(int _WaveForm) { WaveForm = _WaveForm; }
    void set_Level(int _Level) { Level = _Level; }

    int get_Frequency(void) { return Frequency; }
    int get_WaveForm(void) { return WaveForm; }
    int get_Level(void) { return Level; }

private:
    static void pio_irq_handler() {
        if (pio0_hw->irq & 2) {                                                     // test if irq 0 was raised
             switch (SW_3way) {
                case 0b010:                                                         // Top: Frequency range 0 to 999
                    Frequency--;
                    if ( Frequency < 0 ) { Frequency = 999; }
                    UpdateReq |= 0b010;                                            // Flag to update the frequency
                    break;
                case 0b001:                                                         // Bottom : Level range 0 to 99
                    Level--;
                    if ( Level < 0 ) { Level = 99; }
                    UpdateReq |= 0b001;                                             // Flag to update the level
                    break;
                case 0b011:                                                         // Middle: WaveForm range 0 to 4
                    WaveForm--;
                    if ( WaveForm < 0 ) { WaveForm = 8; }
                    UpdateReq |= 0b100;                                             // Flag to update the waveform
             }
        }
        if (pio0_hw->irq & 1) {                                                     // test if irq 1 was raised
             switch (SW_3way) {
                case 0b010:                                                         // Top: Frequency range 0 to 999
                    Frequency++;
                    if ( Frequency > 999 ) { Frequency = 0; }
                    UpdateReq |= 0b010;                                             // Flag to update the frequency
                    break;
                case 0b001:                                                         // Bottom : Level range 0 to 99
                    Level++;
                    if ( Level > 99 ) { Level = 0; }
                    UpdateReq |= 0b001;                                             // Flag to update the level
                    break;
                case 0b011:                                                         // Middle: WaveForm range 0 to 4
                    WaveForm++;
                    if ( WaveForm > 8 ) { WaveForm = 0; }
                    UpdateReq |= 0b100;                                             // Flag to update the waveform
             }
        }
        pio0_hw->irq = 3;                                                           // clear both interrupts
    }

    PIO pio;                                                                        // the pio instance
    uint sm;                                                                        // the state machine
    static int Frequency;
    static int WaveForm;
    static int Level;
};
// Global Var...
int RotaryEncoder::Frequency;                        // Initialize static members of class Rotary_encoder
int RotaryEncoder::WaveForm;                         // Initialize static members of class Rotary_encoder
int RotaryEncoder::Level;                            // Initialize static members of class Rotary_encoder

class blink_forever {                                                               // Class to initialise a state macne to blink a GPIO pin
public:
    blink_forever(PIO pio, uint sm, uint offset, uint pin, uint freq, uint blink_div) {
        blink_program_init(pio, sm, offset, pin, blink_div);
        pio_sm_set_enabled(pio, sm, true);
        printf("PIO:0 SM:%d - Blink @ %dHz\n", sm, freq);
    }
};

void WriteCathodes (int Data) {
// Create bit pattern on cathode GPIO's corresponding to the Data input...
    int  shifted;
    shifted = Data ;
    gpio_put(NixieCathodes[0], shifted %2) ;
    shifted = shifted /2 ;
    gpio_put(NixieCathodes[1], shifted %2);
    shifted = shifted /2;
    gpio_put(NixieCathodes[2], shifted %2);
    shifted = shifted /2;
    gpio_put(NixieCathodes[3], shifted %2);
}

bool Repeating_Timer_Callback(struct repeating_timer *t) {
    // Scans the Nixie Anodes, and transfers data from the Nixie Buffers to the Cathodes.
    switch (ScanCtr) {
        case 0:
            gpio_put(NixieAnodes[2], 0) ;                               // Turn off previous anode
            WriteCathodes(NixieBuffer[0]);                              // Set up new data on cathodes (Units)
            gpio_put(NixieAnodes[0], 1) ;                               // Turn on current anode
            break;
        case 1:
            gpio_put(NixieAnodes[0], 0) ;                               // Turn off previous anode
            WriteCathodes(NixieBuffer[1]);                              // Set up new data on cathodes (10's)
            gpio_put(NixieAnodes[1], 1) ;                               // Turn on current anode
            break;
        case 2:
            gpio_put(NixieAnodes[1], 0) ;                               // Turn off previous anode
            WriteCathodes(NixieBuffer[2]);                              // Set up new data on cathodes (100's)
            gpio_put(NixieAnodes[2], 1) ;                               // Turn on current anode.
    }
    ScanCtr++;
    if ( ScanCtr > 2 ) { ScanCtr = 0; }                                 // Bump and Wrap the counter
    return true;
}

void WaveForm_update (int _value) {
    int i;
    float a,b,c,d,e;
    PIO pio = pio0;
    switch (_value) {
        case 0:
            printf("Waveform: %03d - Sine: Fundamental\n",_value);
            for (i=0; i<(sine_table_size); i++) {
            // Note: 6.283 = 2*Pi
        //      raw_sin[i] = (int)(2047 * sin((float)i*6.283/(float)sine_table_size) + 2047);   // 12 bit
        //      DAC_data[i] = (int)(16 * sin((float)i*6.283/(float)sine_table_size) + 16);      // Memory alligned 5 bit
                DAC_data[i] = (int)(128 * sin((float)i*6.283/(float)sine_table_size) + 128);      // Memory alligned 8 bit
            }        
            break;
        case 1:
            printf("Waveform: %03d - Sine: Fundamental + harmonic 3\n",_value);
            for (i=0; i<(sine_table_size); i++) {
                a = 127 * sin((float)6.283*i / (float)sine_table_size);                     // Fundamental frequency
                b = 127/3 * sin((float)6.283*3*i / (float)sine_table_size);                 // 3rd harmonic
                DAC_data[i] = (int)(a+b)+127;                                               // Sum harmonics and add vertical offset
            }        
            break;
        case 2:
            printf("Waveform: %03d - Sine: Fundamental + harmonics 3 and 5\n",_value);
            for (i=0; i<(sine_table_size); i++) {
                a = 127 * sin((float)6.283*i / (float)sine_table_size);                     // Fundamental frequency
                b = 127/3 * sin((float)6.283*3*i / (float)sine_table_size);                 // 3rd harmonic
                c = 127/5 * sin((float)6.283*5*i / (float)sine_table_size);                 // 5th harmonic
                DAC_data[i] = (int)(a+b+c)+127;                                             // Sum harmonics and add vertical offset
            }        
            break;
        case 3:
            printf("Waveform: %03d - Sine: Fundamental + harmonics 3,5 and 7\n",_value);
            for (i=0; i<(sine_table_size); i++) {
                a = 127 * sin((float)6.283*i / (float)sine_table_size);                     // Fundamental frequency
                b = 127/3 * sin((float)6.283*3*i / (float)sine_table_size);                 // 3rd harmonic
                c = 127/5 * sin((float)6.283*5*i / (float)sine_table_size);                 // 5th harmonic
                d = 127/7 * sin((float)6.283*7*i / (float)sine_table_size);                 // 7th harmonic
                DAC_data[i] = (int)(a+b+c+d)+127;                                           // Sum harmonics and add vertical offset
            }        
            break;
        case 4:
            printf("Waveform: %03d - Sine: Fundamental + harmonic 3, 5, 7 and 9\n",_value);
            for (i=0; i<(sine_table_size); i++) {
                a = 127 * sin((float)6.283*i / (float)sine_table_size);                     // Fundamental frequency
                b = 127/3 * sin((float)6.283*3*i / (float)sine_table_size);                 // 3rd harmonic
                c = 127/5 * sin((float)6.283*5*i / (float)sine_table_size);                 // 5th harmonic
                d = 127/7 * sin((float)6.283*7*i / (float)sine_table_size);                 // 7th harmonic
                e = 127/9 * sin((float)6.283*9*i / (float)sine_table_size);                 // 9th harmonic
                DAC_data[i] = (int)(a+b+c+d+e)+127;                                         // Sum harmonics and add vertical offset
            }        
            break;
        case 5: printf("Waveform: %03d - Square\n",_value);
            for (i=0; i<(sine_table_size/2); i++){
                DAC_data[i] = 0;                                                            // First half:  low
                DAC_data[i+sine_table_size/2] = 255;                                        // Second half: high
            }
            break;
        case 6:
            printf("Waveform: %03d - Sawtooth (rising)\n",_value);
            for (i=0; i<(sine_table_size); i++) {
                DAC_data[i] = i;                                                            // 8 bit
            }
            break;
        case 7: printf("Waveform: %03d - Sawtooth (falling)\n",_value);
            for (i=0; i<(sine_table_size); i++) {
                DAC_data[i] = 32-i;                                                         // 8 bit
            }
            break;
        case 8: printf("Waveform: %03d - Triangle\n",_value);
            for (i=0; i<(sine_table_size/2); i++){
                DAC_data[i] = i*2;                                                          // First half:  slope up
                DAC_data[i+sine_table_size/2] = 255-i*2;                                    // Second half: slope down
            }
            break;
    }

/* DEBUG - CONFIRM MEMORY ALIGNMENT...
    printf("\nConfirm memory alignment...\nBeginning: %x", &DAC_data[0]);
    printf("\nFirst: %x", &DAC_data[1]);
    printf("\nSecond: %x\n\n", &DAC_data[2]); */

    NixieBuffer[0] = _value % 10 ;                                  // First Nixie ( 1's )
    _value /= 10 ;                                                  // finished with _value, so ok to trash it. _value=>10's
    NixieBuffer[1] = _value % 10 ;                                  // Second Nixie ( 10's )
    _value /= 10 ;                                                  // _value=>100's
    NixieBuffer[2] = 10 ;                                           // Blank Third Nixie ( 100's )
}

int main() {
    static const float blink_freq = 16000;                                      // Reduce SM clock to keep flash visible...
    float blink_div = (float)clock_get_hz(clk_sys) / blink_freq;                //   ... calculate the required blink SM clock divider
    static const float rotary_freq = 16000;                                     // Clock speed reduced to eliminate rotary encoder jitter...
    float rotary_div = (float)clock_get_hz(clk_sys) / rotary_freq;              //... then calculate the required rotary encoder SM clock divider
    float DAC_freq, DAC_div;

    set_sys_clock_khz(280000, true);                                            // Overclocking the core by a factor of 2 allows 1MHz from DAC
    stdio_init_all();                                                           // needed for printf

// Set up the GPIO pins...
    const uint Onboard_LED = PICO_DEFAULT_LED_PIN;                              // Debug use - intialise the Onboard LED...
    gpio_init(Onboard_LED);
    gpio_set_dir(Onboard_LED, GPIO_OUT);
    // Initialise the Nixie cathodes...
    for ( uint i = 0; i < sizeof(NixieCathodes) / sizeof( NixieCathodes[0]); i++ ) {
        gpio_init(NixieCathodes[i]);
        gpio_set_dir(NixieCathodes[i], GPIO_OUT);                               // Set as output
    }
    // Initialise the Nixe anodes...
    for ( uint i = 0; i < sizeof(NixieAnodes) / sizeof( NixieAnodes[0]); i++ ) {
        gpio_init(NixieAnodes[i]);
        gpio_set_dir(NixieAnodes[i], GPIO_OUT);                                 // Set as output
    }
    // Initialise the rotary encoder...
    for ( uint i = 0; i < sizeof(RotaryEncoder) / sizeof( EncoderPorts[0]); i++ ) {
        gpio_init(EncoderPorts[i]);
        gpio_set_dir(EncoderPorts[i], GPIO_IN);                                 // Set as input
        gpio_pull_up(EncoderPorts[i]);                                          // Enable pull up
    }
    // Initialise the 2-way switch inputs...
    gpio_init(SW_2way_1);
    gpio_set_dir(SW_2way_1, GPIO_IN);
    gpio_pull_up(SW_2way_1);
    // Initialise the 3-way switch inputs...
    gpio_init(SW_3way_1);
    gpio_set_dir(SW_3way_1, GPIO_IN);
    gpio_pull_up(SW_3way_1);
    gpio_init(SW_3way_2);
    gpio_set_dir(SW_3way_2, GPIO_IN);
    gpio_pull_up(SW_3way_2);

    RotaryEncoder my_encoder(16, rotary_freq);                                  // the A of the rotary encoder is connected to GPIO 16, B to GPIO 17

// Confirm memory alignment
    printf("\nConfirm memory alignment...\nBeginning: %x", &DAC_data[0]);
    printf("\nFirst: %x", &DAC_data[1]);
    printf("\nSecond: %x\n\n", &DAC_data[2]);

// Set up the State machines...
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &pio_blink_program);
    blink_forever my_blinker(pio, 0, offset, 25, blink_freq, blink_div);        // SM0=>onboard LED

// Select a PIO and find a free state machine on it (erroring if there are none).
// Configure the state machine to run our program, and start it, using the helper function we included in our .pio file.
    pio = pio1;

    offset = pio_add_program(pio, &pio_FastDAC_program);
    uint sm_FastDAC = pio_claim_unused_sm(pio, true);
    pio_FastDAC_program_init(pio, sm_FastDAC, offset, 2);

    offset = pio_add_program(pio, &pio_SlowDAC_program);
    uint sm_SlowDAC = pio_claim_unused_sm(pio, true);
    pio_SlowDAC_program_init(pio, sm_SlowDAC, offset, 2);

// Get 2 x free DMA channels for the Fast DAC - panic() if there are none
    int fast_ctrl_chan = dma_claim_unused_channel(true);
    int fast_data_chan = dma_claim_unused_channel(true);
    printf("FastDAC:\n");
    printf("PIO:%d SM:%d\n", 1, sm_FastDAC);
    printf("DMA:%d ctrl channel\n", fast_ctrl_chan);
    printf("DMA:%d data channel\n\n", fast_data_chan);

// Setup the Fast DAC control channel...
// The control channel transfers two words into the data channel's control registers, then halts. The write address wraps on a two-word
// (eight-byte) boundary, so that the control channel writes the same two registers when it is next triggered.
   dma_channel_config fc = dma_channel_get_default_config(fast_ctrl_chan);  // default configs
   channel_config_set_transfer_data_size(&fc, DMA_SIZE_32);                 // 32-bit txfers
   channel_config_set_read_increment(&fc, false);                           // no read incrementing
   channel_config_set_write_increment(&fc, false);                          // no write incrementing
   dma_channel_configure(
       fast_ctrl_chan,
       &fc,
       &dma_hw->ch[fast_data_chan].al1_transfer_count_trig,                 // txfer to transfer count trigger
       &transfer_count,
       1,
       false
   );

// Setup the Fast DAC data channel...
// 32 bit transfers. Read address increments after each transfer.
    fc = dma_channel_get_default_config(fast_data_chan);
    channel_config_set_transfer_data_size(&fc, DMA_SIZE_32);                // 32-bit txfers
    channel_config_set_read_increment(&fc, true);                           // increment the read adddress, don't increment write address
    channel_config_set_write_increment(&fc, false);
    channel_config_set_dreq(&fc, pio_get_dreq(pio, sm_FastDAC, true));      // Transfer when PIO SM TX FIFO has space
    channel_config_set_chain_to(&fc, fast_ctrl_chan);                       // chain to the controller DMA channel
    channel_config_set_ring(&fc, false, 9);                                 // 1 << 9 byte boundary on read ptr    
                                                                            // set wrap boundary. This is why we needed alignment!
    dma_channel_configure(
        fast_data_chan,                                                     // Channel to be configured
        &fc,                                                                // The configuration we just created
        &pio->txf[sm_FastDAC],                                              // Write to FIFO
        DAC_data,                                                           // The initial read address (AT NATURAL ALIGNMENT POINT)
        sine_table_size,                                                    // Number of transfers; in this case each is 2 byte.
        false                                                               // Don't start immediately.
    );

// Get 2 x free DMA channels for the Slow DAC - panic() if there are none
    int slow_ctrl_chan = dma_claim_unused_channel(true);
    int slow_data_chan = dma_claim_unused_channel(true);
    printf("SlowDAC:\n");
    printf("PIO:%d SM:%d\n", 1, sm_SlowDAC);
    printf("DMA:%d ctrl channel\n", slow_ctrl_chan);
    printf("DMA:%d data channel\n\n", slow_data_chan);

// Setup the Slow DAC control channel...
// The control channel transfers two words into the data channel's control registers, then halts. The write address wraps on a two-word
// (eight-byte) boundary, so that the control channel writes the same two registers when it is next triggered.
   dma_channel_config sc = dma_channel_get_default_config(slow_ctrl_chan);  // default configs
   channel_config_set_transfer_data_size(&sc, DMA_SIZE_32);                 // 32-bit txfers
   channel_config_set_read_increment(&sc, false);                           // no read incrementing
   channel_config_set_write_increment(&sc, false);                          // no write incrementing
   dma_channel_configure(
       slow_ctrl_chan,
       &sc,
       &dma_hw->ch[slow_data_chan].al1_transfer_count_trig,                 // txfer to transfer count trigger
       &transfer_count,
       1,
       false
   );

// Setup the slow DAC data channel...
// 32 bit transfers. Read address increments after each transfer.
    sc = dma_channel_get_default_config(slow_data_chan);
    channel_config_set_transfer_data_size(&sc, DMA_SIZE_32);                // 32-bit txfers
    channel_config_set_read_increment(&sc, true);                           // increment the read adddress, don't increment write address
    channel_config_set_write_increment(&sc, false);
    channel_config_set_dreq(&sc, pio_get_dreq(pio, sm_SlowDAC, true));      // Transfer when PIO SM TX FIFO has space
    channel_config_set_chain_to(&sc, slow_ctrl_chan);                       // chain to the controller DMA channel
    channel_config_set_ring(&sc, false, 9);                                 // 1 << 9 byte boundary on read ptr    
                                                                            // set wrap boundary. This is why we needed alignment!
    dma_channel_configure(
        slow_data_chan,                                                     // Channel to be configured
        &sc,                                                                // The configuration we just created
        &pio->txf[sm_SlowDAC],                                              // Write to FIFO
        DAC_data,                                                           // The initial read address (AT NATURAL ALIGNMENT POINT)
        sine_table_size,                                                    // Number of transfers; in this case each is 2 byte.
        false                                                               // Don't start immediately.
    );

// Create a repeating timer that calls Repeating_Timer_Callback.
// If the delay is > 0 then this is the delay between the previous callback ending and the next starting. If the delay is negative
// then the next call to the callback will be exactly 7ms after the start of the call to the last callback.
  struct repeating_timer timer;
    add_repeating_timer_ms(-7, Repeating_Timer_Callback, NULL, &timer);         // 7ms - Short enough to avoid Nixie tube flicker
                                                                                //       Long enough to avoid Nixie tube bluring

    my_encoder.set_Frequency(100);                                              // Default: 100Hz
    my_encoder.set_WaveForm(0);                                                 // Default: Sine wave
    my_encoder.set_Level(50);                                                   // Default: 50%
    UpdateReq = 0b0111;                                                         // Set flags to load all default values

    while (true) {                                                              // Infinite loop
        if (UpdateReq) {
        // Falls through here when any of the rotary encoder values change...
            if (UpdateReq & 0b010) {                                            // Frequency has changed
                NixieVal = my_encoder.get_Frequency();                          // Value in range 0->999
                Frequency = NixieVal;
                if (SW_2way != 0) { Frequency *= 1000; }                        // Scale by 1K if required
                if (Frequency >= 17) {
                // If DAC_div exceeds 2^16 (65,536), the registers wrap around, and the State Machine clock will be incorrect.
                // A slower version of the DAC State Machine is used for frequencies below 17Hz, allowing the DAC_div to be kept
                // within range.
                    // FastDAC ( 17Hz=>1Mhz )
                    DAC_freq = Frequency*256;                                    // Target frequency...
                    DAC_div = (float)clock_get_hz(clk_sys) / DAC_freq;          //   ...calculate the required rotary encoder SM clock divider
                    pio_sm_set_clkdiv(pio1, sm_FastDAC, DAC_div );
                    pio_sm_set_enabled(pio, sm_SlowDAC, false);                 // Stop the SlowDAC State MAchine
                    pio_sm_set_enabled(pio, sm_FastDAC, true);                  // Start the FastDAC State Machine
                    dma_start_channel_mask(1u << fast_ctrl_chan);               // Start the FastDAC DMA channel
                } else {
                    // SlowDAC ( 1Hz=>16Hz )
                    DAC_freq = Frequency*256;                                    // Target frequency...
                    DAC_div = (float)clock_get_hz(clk_sys) / DAC_freq;          //   ...calculate the required rotary encoder SM clock divider
                    DAC_div = DAC_div / 32;                                     // Adjust to keep DAC_div within useable range
                    pio_sm_set_clkdiv(pio1, sm_SlowDAC, DAC_div );
                    pio_sm_set_enabled(pio, sm_FastDAC, false);                 // Stop the FastDAC State Machine
                    pio_sm_set_enabled(pio, sm_SlowDAC, true);                  // Start the SlowDAC State MAchine
                    dma_start_channel_mask(1u << slow_ctrl_chan);               // Start the SlowDAC DMA channel
                }
                printf("Rotation: %03d - SM Div: %8.4f - SM Clk: %07.0gHz - Fout: %3.0f",NixieVal, DAC_div, DAC_freq, DAC_freq/256);
//              printf("Frequency: %03d",NixieVal);
                if (Frequency < 1000) { printf("Hz\n");  }
                else                  { printf("KHz\n"); }
                NixieBuffer[0] = NixieVal % 10 ;                                // First Nixie ( 1's )
                NixieVal /= 10 ;                                                // finished with NixieVal, so ok to trash it. NixieVal=>10's
                NixieBuffer[1] = NixieVal % 10 ;                                // Second Nixie ( 10's )
                NixieVal /= 10 ;                                                // teNixieValmp=>100's
                NixieBuffer[2] = NixieVal % 10 ;                                // Third Nixie ( 100's )
            }
            if (UpdateReq & 0b100) {                                            // Waveform has changed
                NixieVal  = my_encoder.get_WaveForm();
                WaveForm_update(NixieVal);
                NixieBuffer[0] = NixieVal % 10 ;                                // First Nixie ( 1's )
                NixieVal /= 10 ;                                                // finished with NixieVal, so ok to trash it. NixieVal=>10's
                NixieBuffer[1] = NixieVal % 10 ;                                // Second Nixie ( 10's )
                NixieVal /= 10 ;                                                // NixieVal=>100's
                NixieBuffer[2] = 10 ;                                           // Blank Third Nixie ( 100's )
            }
            if (UpdateReq & 0b001) {                                            // Level has changed
                NixieVal  = my_encoder.get_Level();
                printf("Level: %02d\n",NixieVal);
                NixieBuffer[0] = NixieVal % 10 ;                                // First Nixie ( 1's )
                NixieVal /= 10 ;                                                // finished with teNixieValmp, so ok to trash it. NixieVal=>10's
                NixieBuffer[1] = NixieVal % 10 ;                                // Second Nixie ( 10's )
                NixieVal /= 10 ;                                                // NixieVal=>100's
                NixieBuffer[2] = 10 ;                                           // Blank Third Nixie ( 100's )
            }
            UpdateReq = 0;                                                      // All up to date, so clear the flag
        }
        // Get 2 way toggle switch status...
        SW_2way = gpio_get(SW_2way_1);                                          // True=KHz, False=Hz
        if (SW_2way != Last_SW_2way) {
            if (SW_2way == 0) { printf("Frequency: Hz\n");  }
            else              { printf("Frequency: KHz\n"); }
            Last_SW_2way = SW_2way;
            UpdateReq = 0b010;                                                  // Force frequency update to load new value to DAC + SM
        }

        // Get 3 way toggle switch status...
        SW_3way = (gpio_get(SW_3way_1)<<1) + (gpio_get(SW_3way_2));
        if (SW_3way != Last_SW_3way) {
            switch (SW_3way) {
                case 0b010:                                                     // SW=>Top position
                    printf("Frequency: %03d Hz\n",my_encoder.get_Frequency());
                    break;
                case 0b011:                                                     // SW=>Middle position
                    WaveForm_update(my_encoder.get_WaveForm());
                    break;
                case 0b001:                                                     // SW=>Bottom position
                    printf("Level: %02d\n",my_encoder.get_Level());
                    break;
//              case 0b000: printf("Undefined:\n");                             // Impossible combination
            }
        Last_SW_3way = SW_3way;
        }
    }
}
