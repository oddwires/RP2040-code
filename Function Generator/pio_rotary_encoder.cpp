#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "pio_rotary_encoder.pio.h"
#include "pio_blink.pio.h"

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq);

class RotaryEncoder {                                                               // class to read the rotation of the rotary encoder
public:
    // constructor
    // rotary_encoder_A is the pin for the A of the rotary encoder.
    // The B of the rotary encoder has to be connected to the next GPIO.
    RotaryEncoder(uint rotary_encoder_A) {
        uint8_t rotary_encoder_B = rotary_encoder_A + 1;
        PIO pio = pio0;                                                             // Use pio 0
        uint8_t sm = 0;                                                             // Use state machine 0
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
    }

    void set_rotation(int _rotation) {                                              // set the current rotation to a specific value
        rotation = _rotation;
    }

    int get_rotation(void) {                                                        // get the current rotation
        return rotation;
    }

private:
    static void pio_irq_handler() {
        if (pio0_hw->irq & 2) {                                                     // test if irq 0 was raised
            rotation = rotation - 1;
            if ( rotation < 0) { rotation = 999; }
        }
        if (pio0_hw->irq & 1) {                                                     // test if irq 1 was raised
            rotation = rotation + 1;
            if ( rotation > 999 ) { rotation = 0; }
        }
        pio0_hw->irq = 3;                                                           // clear both interrupts
    }

    PIO pio;                                                                        // the pio instance
    uint sm;                                                                        // the state machine
    static int rotation;                                                            // the current location of rotation
};

// Global variables...
int RotaryEncoder::rotation = 0;                    // Initialize static members of class Rotary_encoder
int NixieCathodes[4]    = { 18, 19, 20, 21 };       // GPIO ports connecting to Nixie Cathodes  - Data0=>18     Data3=>21
int NixieAnodes[3]      = { 22, 26, 27 };           // GPIO ports connecting to Nixie Anodes    - Anode0=>22    Anode2=>27
int EncoderPorts[2]     = { 16, 17 };               // GPIO ports connecting to Rotary Encoder  - 16=>Clock     17=>Data
int NixieBuffer[3]      = { 6, 7, 8 };              // Values to be displayed on Nixie tubes    - Tube0=>1's
                                                    //                                          - Tube1=>10's
                                                    //                                          - Tube2=>100's

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

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
    printf("Blinking pin %d at %d Hz\n", pin, freq);
    pio->txf[sm] = clock_get_hz(clk_sys) / (2 * freq);
}

int main() {
    int scan = 0, lastval, temp;

    stdio_init_all();                                                               // needed for printf
    RotaryEncoder my_encoder(16);                                                   // the A of the rotary encoder is connected to GPIO 16, B to GPIO 17
    my_encoder.set_rotation(0);                                                     // initialize the rotatry encoder rotation as 0
    // Iterate through arrays to initialise the GPIO ports...
    for ( uint i = 0; i < sizeof(NixieCathodes) / sizeof( NixieCathodes[0]); i++ ) {
        gpio_init(NixieCathodes[i]);
        gpio_set_dir(NixieCathodes[i], GPIO_OUT);                                   // Set as output
    }
    for ( uint i = 0; i < sizeof(NixieAnodes) / sizeof( NixieAnodes[0]); i++ ) {
        gpio_init(NixieAnodes[i]);
        gpio_set_dir(NixieAnodes[i], GPIO_OUT);                                     // Set as output
    }
    for ( uint i = 0; i < sizeof(RotaryEncoder) / sizeof( EncoderPorts[0]); i++ ) {
        gpio_init(EncoderPorts[i]);
        gpio_set_dir(EncoderPorts[i], GPIO_IN);                                     // Set as input
        gpio_pull_up(EncoderPorts[i]);                                              // Enable pull up
    }
    const uint Onboard_LED = PICO_DEFAULT_LED_PIN;                                  // Debug - also intialise the Onboard LED...
    gpio_init(Onboard_LED);
    gpio_set_dir(Onboard_LED, GPIO_OUT);

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &pio_blink_program);
    printf("Loaded program at %d\n", offset);

    blink_pin_forever(pio, 1, offset, 25, 10);                                      // SM1, onboard LED, 10Hz
//    blink_pin_forever(pio, 0, offset, 0, 3);                                      // Optional: Specify additional SM's, different pins,
//    blink_pin_forever(pio, 2, offset, 11, 1);                                     //    differnt frequencies

    while (true) {                                                                  // infinite loop to print the current rotation
        if (my_encoder.get_rotation() != lastval) {
            temp  = my_encoder.get_rotation();
            printf("rotation=%d\n", temp);
            lastval = temp;
            NixieBuffer[0] = temp % 10 ;                                            // finished with temp, so ok to destroy it
            temp /= 10 ;
            NixieBuffer[1] = temp % 10 ;
            temp /= 10 ;
            NixieBuffer[2] = temp % 10 ;
        }

        if (scan==0) {
            gpio_put(NixieAnodes[2], 0) ;                                           // Turn off previous anode
            WriteCathodes(NixieBuffer[0]);                                          // Set up new data on cathodes (Units)
            gpio_put(NixieAnodes[0], 1) ;                                           // Turn on current anode
        }
        if (scan==1) {
            gpio_put(NixieAnodes[0], 0) ;                                           // Turn off previous anode
            WriteCathodes(NixieBuffer[1]);                                          // Set up new data on cathodes (10's)
            gpio_put(NixieAnodes[1], 1) ;                                           // Turn on current anode
        }
        if (scan==2) {
            gpio_put(NixieAnodes[1], 0) ;                                           // Turn off previous anode
            WriteCathodes(NixieBuffer[2]);                                          // Set up new data on cathodes (100's)
            gpio_put(NixieAnodes[2], 1) ;                                           // Turn on current anode
        }
        scan++;
        if (scan == 3) { scan = 0; }
        sleep_ms(5);
    }
}