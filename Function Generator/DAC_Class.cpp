// Each DAC channel consists of...
//    BitMap data => DMA => FIFO => State Machine => GPIO pins => R-2R module
// Note: The PIO clock dividers are 16-bit integer, 8-bit fractional, with first-order delta-sigma for the fractional divider.
//       This means the clock divisor can vary between 1 and 65536, in increments of 1/256.
//       If DAC_div exceeds 2^16 (65,536), the registers will wrap around, and the State Machine clock will be incorrect.
//       For frequencies below 34Hz, an additional 63 op-code delay is inserted into the State Machine assembler code. This slows
//       down the State Machine operation by a factor of 64, keeping the value of DAC_div within range.

#include "DAC_Class.h"
#include "ClockModule.h"

DAC::DAC(char _name, PIO _pio, uint8_t _GPIO)
{
// DAC Constructor
// Parameters...
//       _name = Name of this DAC channel instance
//       _pio = Required PIO channel
//       _GPIO = Port connecting to the MSB of the R-2R resistor network.
    pio = _pio;
    PIOnum = pio_get_index(pio) ;                                           // Printer friendly value
    GPIO = _GPIO ;                                                          // Initialse class value
    Funct = _Sine_, Freq = 100, Level = 50 ;                                // Start-up default values...
    Range = 1, Harm = 0, DutyC = 50, RiseT = 50, name = _name ;
    name == 'A' ? Phase=0 : Phase=180 ;                                     // Set Phase difference between channels
    int _offset;

    StateMachine = pio_claim_unused_sm(_pio, true);                         // Find a free state machine on the specified PIO - error if there are none.
    ctrl_chan = dma_claim_unused_channel(true);                             // Find 2 x free DMA channels for the DAC (12 available)
    data_chan = dma_claim_unused_channel(true);

    // Configure the state machine to run the DAC program...
    _offset = pio_add_program(_pio, &pio_DAC_program);                      // Use helper function included in the .pio file.
    SM_WrapBot = _offset;
    pio_DAC_program_init(_pio, StateMachine, _offset, _GPIO);
    //  Setup the DAC control channel...
    //  The control channel transfers two words into the data channel's control registers, then halts. The write address wraps on a two-word
    //  (eight-byte) boundary, so that the control channel writes the same two registers when it is next triggered.
    dma_channel_config fc = dma_channel_get_default_config(ctrl_chan);      // default configs
    channel_config_set_transfer_data_size(&fc, DMA_SIZE_32);                // 32-bit txfers
    channel_config_set_read_increment(&fc, false);                          // no read incrementing
    channel_config_set_write_increment(&fc, false);                         // no write incrementing

//   !!! THIS RESTRICTS SPEED TO ~730KHz !!!
//      fc.ctrl |= DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS;                        // Set high priority

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

//   !!! THIS RESTRICTS SPEED TO ~730KHz !!!
//      fc.ctrl |= DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS;                        // Set high priority

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
    DACspeed(Freq * Range) ;                                                // Initialise State Machine clock speed.
};

char* DAC::StatusString() {
// Report the status line for the current DAC object.
    char TmpStr[4];

    if (Range == 1)       strcpy(TmpStr," Hz") ;                            // Assign multiplier suffix
    if (Range == 1000)    strcpy(TmpStr,"KHz") ;
    if (Range == 1000000) strcpy(TmpStr,"MHz") ;

    switch ( Funct ) {                                                      // Calculate status sting...
        case _Sine_:
            sprintf(RetStr,"Channel %c: Freq:%3d%s Phase:%03d Level:%03d Wave:Sine Harmonic:%d\n",name, Freq, TmpStr, Phase, Level, Harm) ;
            return RetStr;
        case _Triangle_:
            if ((RiseT == 0) || (RiseT == 100)) {
                sprintf(RetStr,"Channel %c: Freq:%3d%s Phase:%03d Level:%03d Wave:Sawtooth\n",name, Freq, TmpStr, Phase, Level) ;
                } else {
                sprintf(RetStr,"Channel %c: Freq:%3d%s Phase:%03d Level:%03d Wave:Triangle Rise time:%d%%\n",name, Freq, TmpStr, Phase, Level, RiseT) ;
                }
            return RetStr;
        case _Square_:
            sprintf(RetStr,"Channel %c: Freq:%3d%s Phase:%03d Level:%03d Wave:Square Duty cycle:%d%%\n",name, Freq, TmpStr, Phase, Level, DutyC) ;
            return RetStr;
        default:
            return 0;                                               // Program execution should never get here
    }
}

int DAC::Set(int _type, int _val)
{
// Multi-purpose routine to set various DAC operating values.
// Parameters...
//      _type = operating value to be set (frequency, phase, level, sine, square, triangle)
//      _val  = value for the designated parameter
//    _Result.Val = _val;                                 // Save for SPI display

    switch (_type) {
        case _Freq_:
            Freq  = _val ;                              // Frequency (numeric)
            DACspeed(Freq * Range) ;                    // Update State machine run speed
            if (Range == 1)       strcpy(Suffix," Hz") ;        // Assign multiplier suffix
            if (Range == 1000)    strcpy(Suffix,"KHz") ;
            if (Range == 1000000) strcpy(Suffix,"MHz") ;
            sprintf(RetStr,"Freq: %3d%s",Freq,Suffix);
            break ;
        case _Phase_:
            Phase  = _val ;                             // Phase shift (0->355 degrees)
            DataCalc() ;                                // Recalc Bitmap and apply new phase value
            sprintf(RetStr,"Phase: %3d°",Phase);            
            break ;
        case _Level_:
            if (_val > 100) _val = 100 ;                // Limit max val to 100%
            Level = _val ;
            MCP41020_Write(SelectedChan, Level) ;       // Control byte for the MCP42010 just happens to be the same value as the SelectedChan variable
            sprintf(RetStr,"Level: %3d%%%%",Level);
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
    }
    NixieVal = _val ;                                   // Result for SPI (Nixie) display
    return 0;
}

int DAC::Bump(int _type, int _dirn)
{
// Multi-purpose routine to bump various DAC operating values.
// Parameters...
//      _type = operating value to be bumped (frequency, phase, level, sine, square, triangle, time)
//      _dirn = bump direction for the designated parameter (up, down)
    if (_type == _Freq_) {
        if ((Freq*Range == 1) && (_dirn == _Down)) {                // Attempt to bump below lower limit (1Hz)
            sprintf(RetStr,"Error - Minimum frequency");
            return 0;
        }
        else if ((Freq*Range == MaxFreq) && (_dirn == _Up)) {       // Attempt to bump above upper limit (1MHz)
            sprintf(RetStr,"Error - Maximum frequency");
            return 0;
        } else {                                                    // Not at max or min value...
            Freq += _dirn ;                                         // ... bump
            if ((Freq == 1000) && (_dirn == _Up)) {                 // Range transition point
                Freq = 1 ;                                          // Reset count
                if (Range == 1)         Range = 1000 ;              // either Hz=>KHz
                else if (Range == 1000) Range = 1000000 ;           // or     KHz=>MHz
            }
            if ((Freq==0) && (Range!=1) && (_dirn==_Down)) {        // Range transition point
                Freq = 999 ;                                        // Reset count
                if (Range == 1000)    Range = 1 ;                   // either KHz=>Hz
                else if (Range == 1000000) Range = 1000 ;           // or     MHz=>KHz
            }
            DACspeed(Freq * Range) ;
            if (Range == 1)       strcpy(Suffix," Hz") ;            // Assign multiplier suffix
            if (Range == 1000)    strcpy(Suffix,"KHz") ;
            if (Range == 1000000) strcpy(Suffix,"MHz") ;
            sprintf(RetStr,"Freq: %3d%s",Freq,Suffix);
        }
        NixieVal = Freq ;                                           // Value for SPI (Nixie) display
    }
    if (_type == _Phase_) {
        Phase += _dirn ;
        if (Phase == 360)  Phase = 0 ;                              // Top Endwrap
        if (Phase  < 0  )  Phase = 359 ;                            // Bottom Endwrap
        DataCalc();                                                 // Update Bitmap data to include new DAC phase
        sprintf(RetStr,"Phase: %3d°",Phase);
        NixieVal = Phase ;  }                                       // Value for SPI (Nixie) display
    if (_type == _Level_) {
        Level += _dirn ;
        if (Level > 100) { Level = 0 ;   }                          // Top endwrap
        if (Level < 0  ) { Level = 100 ; }                          // Bottom endwrap
        MCP41020_Write(SelectedChan, Level) ;                       // Control byte for the MCP42010 just happens to be the same value as the SelectedChan variable
        sprintf(RetStr,"Level: %3d%%%%",Level); 
        NixieVal = Level ; }                                        // Value for SPI (Nixie) display
    if (_type == _Square_) {
        DutyC += _dirn ;
        if (DutyC > 100) { DutyC = 0 ;   }                          // Top endwrap
        if (DutyC < 0  ) { DutyC = 100 ; }                          // Bottom endwrap
        DataCalc();                                                 // Update Bitmap with new Duty Cycle value
        sprintf(RetStr,"Duty cycle: %2d",DutyC); 
        NixieVal = DutyC ; }                                        // Value for SPI (Nixie) display
    if (_type == _Triangle_) {
        RiseT += _dirn ;
        if (RiseT > 100) { RiseT = 0 ;   }                          // Top endwrap
        if (RiseT < 0  ) { RiseT = 100 ; }                          // Bottom endwrap
        DataCalc();                                                 // Update Bitmap with new Duty Cycle value
        sprintf(RetStr,"Triangle Rise time: %2d",RiseT);      
        NixieVal = RiseT ; }                                        // Value for SPI (Nixie) display
    if (_type == _Sine_) {
        Harm += _dirn ;
        if (Harm > 10) { Harm = 0 ;   }                             // Top endwrap
        if (Harm < 0 ) { Harm = 9 ;   }                             // Bottom endwrap
        DataCalc();                                                 // Update Bitmap with new Sine harmonic value
        sprintf(RetStr,"Sine Harmonic: %2d",Harm);
        NixieVal = Harm ; }                                         // Value for SPI (Nixie) display
    return 0;
}

void DAC::DACspeed(int _frequency)
{
// If DAC_div exceeds 2^16 (65,536), the registers wrap around, and the State Machine clock will be incorrect.
// A slow version of the DAC State Machine is used for frequencies below 17Hz, allowing the value of DAC_div to
// be kept within the working range.
    float DAC_freq = _frequency * BitMapSize;                               // Target frequency...
    DAC_div = 2 * (float)clock_get_hz(clk_sys) / DAC_freq;                  // ...calculate the PIO clock divider required for the given Target frequency
    float Fout = 2 * (float)clock_get_hz(clk_sys) / (BitMapSize * DAC_div); // Actual output frequency
    if (_frequency >= 34) {                                                 // Fast DAC ( Frequency range from 34Hz to 999Khz )
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
}

void DAC::DataCalc()
{
    // Calculate the bitmap for the various waveform outputs...
    int i, j, v_offset = BitMapSize/2 - 1;                                                        // Shift sine waves up above X axis
    int _phase;
    float a,b,x1,x2,g1,g2;

    // Scale the phase shift to match data size...    
    _phase = Phase * BitMapSize / 360 ;                                                           // Input  range: 0 -> 360 (degrees)
                                                                                                    // Output range: 0 -> 255 (bytes)
    switch (Funct) {
        case _Sine_:
            Harm = Harm % 11;                                                                     // Sine harmonics cycles after 10
            for (i=0; i<BitMapSize; i++) {
            // Add the phase offset and wrap data beyond buffer end back to the buffer start...
                j = ( i + _phase ) % BitMapSize;                                                   // Horizontal index
                a = v_offset * sin((float)_2Pi*i / (float)BitMapSize);                             // Fundamental frequency...
                if (Harm >= 1)  { a += v_offset/3  * sin((float)_2Pi*3*i  / (float)BitMapSize); }  // Add  3rd harmonic
                if (Harm >= 2)  { a += v_offset/5  * sin((float)_2Pi*5*i  / (float)BitMapSize); }  // Add  5th harmonic
                if (Harm >= 3)  { a += v_offset/7  * sin((float)_2Pi*7*i  / (float)BitMapSize); }  // Add  7th harmonic
                if (Harm >= 4)  { a += v_offset/9  * sin((float)_2Pi*9*i  / (float)BitMapSize); }  // Add  9th harmonic
                if (Harm >= 5)  { a += v_offset/11 * sin((float)_2Pi*11*i / (float)BitMapSize); }  // Add 11th harmonic
                if (Harm >= 6)  { a += v_offset/13 * sin((float)_2Pi*13*i / (float)BitMapSize); }  // Add 13th harmonic
                if (Harm >= 7)  { a += v_offset/15 * sin((float)_2Pi*15*i / (float)BitMapSize); }  // Add 15th harmonic
                if (Harm >= 8)  { a += v_offset/17 * sin((float)_2Pi*17*i / (float)BitMapSize); }  // Add 17th harmonic
                if (Harm >= 9)  { a += v_offset/19 * sin((float)_2Pi*19*i / (float)BitMapSize); }  // Add 19th harmonic
                if (Harm >= 10) { a += v_offset/20 * sin((float)_2Pi*20*i / (float)BitMapSize); }  // Add 21st harmonic
                DAC_data[j] = (int)(a)+v_offset;                                                   // Sum all harmonics and add vertical offset
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
}

// The following function is not a member of the DAC class, as it operates across the two DAC objects simultaneously. 
// Code is included here for the purposes of clarity.

void PhaseLock( DAC DACobj[2] )
{
// Phase lock the two DAC channels...
//  Parameter...
//      DACobj[2] = Array containing the two DAC objects.

    // Disable the Ctrl channels...
    hw_clear_bits(&dma_hw->ch[DACobj[_DAC_A].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    hw_clear_bits(&dma_hw->ch[DACobj[_DAC_B].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);

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
}
