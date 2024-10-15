// TBD: 1) SPI read connection
//      2) Capacitors on op-amps

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#include "GPIO+Definitions.h"
#include "DAC_Class.h"
#include "SPI_Utils.h"
#include "ClockModule.h"
#include "Demo.h"

int Hours=0, Mins=0, Secs=0, LEDctr=0, Angle, StartX, StartY, Radius ;
float Radians ;
int tmp ;
char Margin[MWidth+1], MarginVW[MWidth+1] ;                                             // Fixed Width & Variable Width strings to create a fixed margin
unsigned short DAC_channel_mask = 0 ;                                                   // Binary mask to simultaneously start all DMA channels
const uint32_t transfer_count = BitMapSize ;                                            // Number of DMA transfers per event
int ParmCnt = 0, Parm[4], WaveForm_Type ;                                               // Storage for 4 command line parameters
int SelectedChan, c, i = 0, dirn = 1, Value;
float MaxDACfreq ;
char inStr[30], outStr[2500], LastCmd[30] ;                                             // outStr large enough to contain the HelpText string
Result _Result;                                                                         // Custom data structure to store Console and SPI data output
struct repeating_timer timer;

void ClockModule(DAC DACobj[] );                                                        // Forward declaration

bool Repeating_Timer_Callback(struct repeating_timer *t) {
    // Routine called 5 times per second...
    int i, steps=64, MidX=128, MidY=128 ;
    LEDctr --  ;
    if (LEDctr>0) {                                     
        // LED off, and no change to the time for 4 out of 5 cycles...
        gpio_put(PICO_DEFAULT_LED_PIN, 0);                                              // LED is connected to PICO_DEFAULT_LED_PIN
    } else {
        // Falls through here once per second. 
        LEDctr = 5 ;
        gpio_put(PICO_DEFAULT_LED_PIN, 1);                                              // LED is connected to PICO_DEFAULT_LED_PIN

        // Bump the clock...
        if ((++Secs)>59) Secs=0 ;                                                       // Always bump seconds
        if (Secs==0) { if ((++Mins)>59 ) Mins=0 ;  }                                    // Bump minutes when seconds = 0
        if ((Mins==0) && (Secs==0)) { if ((++Hours)>24) Hours=0 ; }                     // Bump hours when minutes and seconds = 0

        // Calculate seconds hand...
        i=0, Radius=127 ;                                                               // Radius=Length of seconds hand
        Angle=270-(Secs*6) ;                                                            // Angle in degrees, shifted 90 degree anti-clockwise
        Radians=Angle*3.14159/180 ;                                                     // Angle in radians
        StartX=Radius*cos(Radians)+MidX ;
        StartY=Radius*sin(Radians)+MidY ;
        while(i<steps) { HandsX[i]=StartX+i*(MidX-StartX)/steps ;
                         HandsY[i]=StartY+i*(MidY-StartY)/steps ;
                         i++ ; }
        // Calculate minutes hand...
        i=0, Radius=95 ;                                                                // Radius=Length of minutes hand
        Angle=270-(Mins*6) ;                                                            // Angle in degrees, shifted 90 degree anti-clockwise
        Radians=Angle*3.14159/180 ;                                                     // Angle in radians
        StartX=Radius*cos(Radians)+MidX ;
        StartY=Radius*sin(Radians)+MidY ;
        i=0 ;
        while(i<steps) { HandsX[i+steps]=StartX+i*(MidX-StartX)/steps ;
                         HandsY[i+steps]=StartY+i*(MidY-StartY)/steps ;
                         i++ ; }
        // Calculate hours hand...
        i=0, Radius=64 ;                                                                // Radius=Length of hours hand
        // Note: Hours hand progresses between hours in 5 partial increments, each increment measuring 12 minutes.
        //       Each 12 minute increment adds an additional 6 degrees of rotation to the hours hand.
        Angle=5*(270-(((Hours%12)*6)+(Mins/12)%5)) ;                                    // Angle in degrees, shifted 90 degree anti-clockwise,
                                                                                        //   and scaled by 5 to provide range 0=>12
        Radians=Angle*3.14159/180 ;                                                     // Angle in radians
        StartX=Radius*cos(Radians)+MidX ;
        StartY=Radius*sin(Radians)+MidY ;
        while(i<steps) { HandsX[i+2*steps]=StartX+i*(MidX-StartX)/steps ;
                         HandsY[i+2*steps]=StartY+i*(MidY-StartY)/steps ;
                         i++ ; }

        //  printf("%s%d:%d:%d - %d\n",Margin,Hours,Mins,Secs,tmp) ;                    // Debug
    }
    return true;
}

void VerText () {
    // Print version info aligned to current margin settings...
    strcpy(MarginVW,Margin);                                                            // Re-initialise Variable Width margin...
    tmp = strlen(inStr) ;                                                               // Get number of command line characters
    if (tmp != 0) tmp ++;                                                               // Bump to allow for cursor.
                                                                                        // Note: If called at Start-up there will be no input characters
    MarginVW[MWidth - tmp] = '\0' ;                                                     // Calculate padding required for command characters and cursor
    sprintf(_Result.Txt, "%s|----------------------|\n"
                         "%s|  Function Generator  |\n"
                         "%s|     Version 1.00     |\n"
                         "%s| 26th September 2023  |\n"
                         "%s|----------------------|\n", 
                         MarginVW, Margin, Margin, Margin, Margin ) ;
}

void HlpText () {
    // Print Help text aligned to current margin settings...
    // Note: Following string requires '%%%%' to print '%'
    //       HelpText string is copied to _Result.Txt using sprintf - this reduces '%%%%' to '%%'
    //       _Result.Txt is sent to terminal using printf           - this reduces '%%' to '%'
    strcpy(MarginVW,Margin);                                            // Re-initialise Variable Width margin...
    tmp = strlen(inStr) ;                                               // Get number of command line characters
    if (tmp != 0) tmp ++;                                               // Bump to allow for cursor.
                                                                        // Note: If called at Start-up there will be no input characters
    MarginVW[MWidth - tmp] = '\0' ;                                     // Calculate padding required for command characters and cursor
    sprintf(_Result.Txt, "%sHelp...\n"
                         "%s?            - Help\n"
                         "%sV            - Version\n"
                         "%sI            - Info\n"
                         "%sS            - Status\n"
                         "%sCl           - Clock mode\n"
                         "%sD1           - Demo #01\n"
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
                         "%swhere...\n"
                         "%s\t<A/B/C> = DAC channel A,B or C=both\n"
                         "%s\tnnn     = Three digit numeric value\n",
                         MarginVW, Margin, Margin, Margin, Margin, Margin, Margin, Margin,
                         Margin, Margin, Margin, Margin, Margin, Margin, Margin, Margin,
                         Margin, Margin, Margin, Margin, Margin, Margin, Margin, Margin,
                         Margin, Margin, Margin, Margin, Margin, Margin, Margin, Margin,
                         Margin, Margin ) ;
}

void SysInfo (DAC DACobj[] ) {
    // Print System Info and resource allocation detils, aligned to current margin settings...
    // Note: 1) The following string requires '%%%%' to print '%' because...
    //            a) ResultStr is copied to outStr using sprintf - this reduces '%%%%' to '%%'
    //            b) outStr is sent to terminal using printf     - this reduces '%%' to '%'
    //       2) There seems to be some upper limit to sprintf, so the string is split into two smaller parts.
    strcpy(MarginVW,Margin);                                            // Re-initialise Variable Width margin...
    tmp = strlen(inStr) ;                                               // Get number of command line characters
    if (tmp != 0) tmp ++;                                               // Bump to allow for cursor.
                                                                        // Note: If called at Start-up there will be no input characters
    MarginVW[MWidth - tmp] = '\0' ;                                     // Calculate padding required for command characters and cursor

    // First part of string...
    sprintf(_Result.Txt,"%s|----------------------------------------------------------|\n"
                        "%s| System Info...                                           |\n"
                        "%s|----------------------------------------------------------|\n"
                        "%s|   CPU clock frequency:   %7.3fMHz                      |\n"
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
                        MarginVW,     Margin,           Margin,
                        Margin,   (float)clock_get_hz(clk_sys)/1000000,              
                        Margin,       MaxDACfreq/1000000,
                        Margin,       Margin,           Margin,
                        Margin,       DACobj[_DAC_A].Level,               DACobj[_DAC_B].Level,
                        Margin,       DACobj[_DAC_A].Freq,                DACobj[_DAC_B].Freq,
                        Margin,       DACobj[_DAC_A].Range,               DACobj[_DAC_B].Range,
                        Margin,       DACobj[_DAC_A].Phase,               DACobj[_DAC_B].Phase,
                        Margin,       DACobj[_DAC_A].DutyC,               DACobj[_DAC_B].DutyC,
                        Margin,       DACobj[_DAC_A].Harm,                DACobj[_DAC_B].Harm,
                        Margin,       DACobj[_DAC_A].RiseT,               DACobj[_DAC_B].RiseT
            ) ;
    printf(_Result.Txt) ;                                         // Print first part of string
    // Second part of string...
    sprintf(_Result.Txt,"%s|----------------------------|-----------------------------|\n"
                        "%s|   Divider:      %10.3f |   Divider:       %10.3f |\n"
                        "%s|----------------------------|-----------------------------|\n"
                        "%s|   PIO:               %d     |   PIO:                %d     |\n"
                        "%s|   State machine:     %d     |   State machine:      %d     |\n"
                        "%s|   GPIO:           %d->%d     |   GPIO:          %d->%d     |\n"
                        "%s|  *BM size:    %8d     |  *BM size:     %8d     |\n"
                        "%s|  *BM start:   %x     |  *BM start:    %x     |\n"
                        "%s|   Wrap Bottom:      %2x     |   Wrap Bottom:       %2x     |\n"
                        "%s|   Wrap Top:         %2x     |   Wrap Top:          %2x     |\n"
                        "%s|   DMA ctrl:         %2d     |   DMA ctrl:          %2d     |\n"
                        "%s|   DMA data:         %2d     |   DMA data:          %2d     |\n"
                        "%s|----------------------------|-----------------------------|\n"
                        "%s  *BM = Bit map\n",
                        Margin,
                        Margin,       DACobj[_DAC_A].DAC_div,             DACobj[_DAC_B].DAC_div,
                        Margin,
                        Margin,       DACobj[_DAC_A].PIOnum,              DACobj[_DAC_B].PIOnum,
                        Margin,       DACobj[_DAC_A].StateMachine,        DACobj[_DAC_B].StateMachine,
                        Margin,       DACobj[_DAC_A].GPIO, DACobj[_DAC_A].GPIO+7,
                            DACobj[_DAC_B].GPIO, DACobj[_DAC_B].GPIO+7,
                        Margin,       BitMapSize,                         BitMapSize,
                        Margin, (int)&DACobj[_DAC_A].DAC_data[0],         
                      (int)&DACobj[_DAC_B].DAC_data[0],
                        Margin,       DACobj[_DAC_A].SM_WrapBot,          DACobj[_DAC_B].SM_WrapBot,
                        Margin,       DACobj[_DAC_A].SM_WrapTop,          DACobj[_DAC_B].SM_WrapTop, 
                        Margin,       DACobj[_DAC_A].ctrl_chan,           DACobj[_DAC_B].ctrl_chan,
                        Margin,       DACobj[_DAC_A].data_chan,           DACobj[_DAC_B].data_chan,
                        Margin,       Margin
            ) ;             // Printing the final part of the string is handled by the calling routine.
                            // This prevents the 'unknown command' mechanism from triggering.
}

void getLine() {
    char *pPos = (char *)inStr ;                                                        // Pointer to start of Global input string
    int count = 0 ;
    while(1) {
        c = getchar();
        if (c == eof || c == '\n' || c == '\r') break ;                                 // Non blocking exit
        putchar(c);                                                                     // FullDuplex echo
        *pPos++ = c ;                                                                   // Bump pointer, store character
        count ++ ;
    }
    *pPos = '\0' ;
    return ;
}

void Sweep(DAC DACobj[]) {
    // Continual loop providing the frequency sweep function...
    // Parm[0]=Low frequency, Parm[1]=High frequency, Parm[2]=Scan speed, Parm[3]=Low/High pause
    i = Parm[0];
    for (;;) {
        if (SelectedChan & 0b01) DACobj[_DAC_A].Set(_Freq_,i) ;                         // Set frequency, display status
        if (SelectedChan & 0b10) DACobj[_DAC_B].Set(_Freq_,i) ;                         // Set frequency, display status
        dma_start_channel_mask(DAC_channel_mask);                                       // Atomic restart all 4 DMA channels...
        printf(_Result.Txt) ;                                                           // Update terminal
        _Result.Txt[0] = '\0' ;                                                         // Reset the string variable
        SPI_Display_Write(i);                                                           // Update SPI display
        if (i==Parm[0]) { dirn = 1;
                          printf("pause %dms\n",Parm[3]);
                          sleep_ms(Parm[3]); }
        if (i>=Parm[1]) { dirn =-1; 
                          printf("pause %dms\n",Parm[3]);
                          sleep_ms(Parm[3]); }
        // Disable the Ctrl channels...
        hw_clear_bits(&dma_hw->ch[DACobj[_DAC_A].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
        hw_clear_bits(&dma_hw->ch[DACobj[_DAC_B].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);

        // Abort the data channels...
        dma_channel_abort(DACobj[_DAC_A].data_chan);
        dma_channel_abort(DACobj[_DAC_B].data_chan);

        // Re-enable the Ctrl channels (doesn't restart data transfer)...
        hw_set_bits(&dma_hw->ch[DACobj[_DAC_A].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
        hw_set_bits(&dma_hw->ch[DACobj[_DAC_B].ctrl_chan].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);

        dma_start_channel_mask(DAC_channel_mask);                                       // Atomic restart both DAC channels
        i = i + dirn;
        c = getchar_timeout_us (0);                                                     // Non-blocking char input
        if ((c>=32) & (c<=126)) {
                strcpy(_Result.Txt,"        Exit sweep mode\n") ;                       // Prevents error message
                break; }                                                                // exit on keypress
        sleep_ms(Parm[2]);                                                              // Speed of scan
    }
}

int SetVal(DAC DACobj[], int _Parm, int _Value) {
// Common code for setting DAC operating parameters.
// Handles options to set a specified values, or bump up/down...
// Parameters...
//      DACobj = the selected DAC ( 0/1 or A/B )
//      _Parm  = the required function (frequency, duty cycle, phase, waveform, level)
    strcpy(_Result.Txt, Margin);                                                        // Initialise margin string
    if (inStr[3] == '+') {                                                              // Bump up and grab result for SPI display...
        _Result.Txt[MWidth-strlen(inStr)-1]= '\0';                                      // Truncate margin to allow for typed characters
        if (SelectedChan & 0b01) DACobj[_DAC_A].Bump(_Parm,_Up);
        if (inStr[0] == 'C')     strcat(_Result.Txt,Margin);                            // If there are two lines, add second margin
        if (SelectedChan & 0b10) DACobj[_DAC_B].Bump(_Parm,_Up);
    } else if (inStr[3] == '-') {                                                       // Bump down and grab result for SPI display...
        _Result.Txt[MWidth-strlen(inStr)-1]= '\0';                                      // Truncate margin to allow for typed characters
        if (SelectedChan & 0b01) DACobj[_DAC_A].Bump(_Parm,_Down);
        if (inStr[0] == 'C')     strcat(_Result.Txt,Margin);                            // If there are two lines, add second margin
        if (SelectedChan & 0b10) DACobj[_DAC_B].Bump(_Parm,_Down);
    } else {                                                                            // Not a bump, so set the absolute value from Parm[0]...
        _Result.Txt[MWidth-strlen(inStr)-1]= '\0';                                      // Truncate margin to allow for typed characters
        _Result.Val = Parm[0];                                                          // Grab numeric value for SPI display
        if (SelectedChan & 0b01) DACobj[_DAC_A].Set(_Parm,_Value);
        if (inStr[0] == 'C')     strcat(_Result.Txt,Margin);                            // If there are two lines, add second margin
        if (SelectedChan & 0b10) DACobj[_DAC_B].Set(_Parm,_Value);
    }
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

    dma_start_channel_mask(DAC_channel_mask);                                           // Atomic restart both DAC channels

    return _Value;
}

int main() {
    set_sys_clock_khz(SysClock*1000, true) ;                                            // Set Pico clock speed
    MaxDACfreq = clock_get_hz(clk_sys) / BitMapSize ;                                   // Calculate Maximum DAC output frequency for given CPU clock speed
    stdio_init_all();
    SPI_Init(PIN_CLK, PIN_TX);                                                          // Start the SPI bus

    gpio_init(Display_CS) ;                                                             // Initailse the required GPIO ports...
    gpio_set_dir(Display_CS, GPIO_OUT) ;
    gpio_put(Display_CS, 1) ;                                                           // Chip select is active-low, so initialise to high state
    gpio_init(Level_CS) ;
    gpio_set_dir(Level_CS, GPIO_OUT) ;
    gpio_put(Level_CS, 1) ;                                                             // Chip select is active-low, so initialise to high state
    gpio_init(PICO_DEFAULT_LED_PIN) ;
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT) ;
    
    for (int i=0; i<8; i++) {                                                           // Initialise GPIO ports for DMA channels...
        gpio_set_dir(DAC_A_Start + i, GPIO_OUT);
        gpio_set_slew_rate(DAC_A_Start + i, GPIO_SLEW_RATE_FAST);
        gpio_set_drive_strength(DAC_A_Start + i, GPIO_DRIVE_STRENGTH_12MA);
        gpio_set_dir(DAC_B_Start + i, GPIO_OUT);
        gpio_set_slew_rate(DAC_B_Start + i, GPIO_SLEW_RATE_FAST);
        gpio_set_drive_strength(DAC_B_Start + i, GPIO_DRIVE_STRENGTH_12MA);
    }

    // Note: Need to instantiate DAC objects AFTER the Pico clock speed has been set...
    DAC DACobj[2] { {'A',_PIOnum_,DAC_A_Start }, {'B',_PIOnum_,DAC_B_Start} };          // Define array to contain the two DAC channel objects
                                                                                        // and instantiate the two objects

    memset(Margin,' ',MWidth) ;                                                         // Initialise Fixed Width margin...
    Margin[MWidth] = '\0' ;                                                             //  ... and terminate

    strcpy(LastCmd,"?") ;                                                               // Hitting return will give 'Help'

    SPI_Display_Write(SysClock) ;                                                       // Pico system clock speed (in MHz)
    MCP41020_Write(0x3, 50) ;                                                           // Both channels -> 50% output level

    // Optional start-up behaviour...
    // while (!stdio_usb_connected()) { sleep_ms(100); }                                // Wait for USB connection
    VerText() ;                                                                         // Start-up message to terminal
    printf(_Result.Txt) ;

    // Atomic Restart - starting all 4 DMA channels simultaneously ensures phase sync between both DAC channels
    dma_start_channel_mask(DAC_channel_mask);                                           // Sets the 'Busy' flag in Ctrl reg

    add_repeating_timer_ms(-200, Repeating_Timer_Callback, NULL, &timer) ;              // 5 x per second to blink LED

    while(1) {
        ParmCnt=0, Parm[0]=0,  Parm[1]=0,  Parm[2]=0,  Parm[3]=0 ;                      // Reset all command line parameters
        strcpy(MarginVW,Margin);                                                        // Re-initialise Variable Width margin...
        MarginVW[MWidth] = '\0' ;                                                       //  ... and terminate
        _Result.Txt[0] = '\0' ;                                                         // Reset string
        printf(">") ;                                                                   // Command prompt

        getLine() ;                                                                     // Fetch command line

        // Zero length string = 'CR' pressed...
        if (strlen(inStr) == 0) { strcpy(inStr,LastCmd) ;                               // Repeat last command    
                                  printf("%s", inStr) ; }

        if (strlen(inStr) == 1) {
        // One character commands...
            if (inStr[0] == '?') HlpText() ;                                            // Help text
            if (inStr[0] == 'V') VerText() ;                                            // Version text
            if (inStr[0] == 'S') { 
                strcpy(_Result.Txt, Margin);
                _Result.Txt[MWidth-2] = '\0' ;                                          // Short margin due to existing input char
                strcat(_Result.Txt, DACobj[_DAC_A].StatusString()) ;                    // Can be one or two line result
                strcat(_Result.Txt, Margin);                                            // Full margin due to no input char
                strcat(_Result.Txt, DACobj[_DAC_B].StatusString()) ; 
            }
            if (inStr[0] == 'I') SysInfo(DACobj);                                       // Info
        }

        if (strlen(inStr) == 2) {
        // Two character commands...
            if ((inStr[0]=='C')&(inStr[1]=='l')) ClockModule(DACobj) ;                  // Clock display
            if ((inStr[0]=='D')&(inStr[1]=='1')) Demo_01(DACobj) ;                      // Demo #1 - Sine, Triangle, Square
            if ((inStr[0]=='D')&(inStr[1]=='2')) Demo_02(DACobj) ;                      // Demo #2 - Lissajous
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
                else if ( inStr[i] == ',' ) { ParmCnt++ ; }                             // Next parameter
                else if (isdigit(inStr[i])) { Parm[ParmCnt] *= 10;                      // Next digit. Bump the existing decimal digits
                                              Parm[ParmCnt] += inStr[i] - '0'; }        // Convert character to integer and add
            }
        }

        // Next two chars select the command...
        if ((inStr[1]=='p')&(inStr[2]=='h')) SetVal(DACobj,_Phase_,Parm[0]) ;           // Phase
        if ((inStr[1]=='l')&(inStr[2]=='e')) SetVal(DACobj,_Level_,Parm[0]) ;           // Level
        if ((inStr[1]=='s')&(inStr[2]=='i')) SetVal(DACobj,_Sine_,Parm[0]) ;            // Sine wave (optional harmonic parameter)
        if ((inStr[1]=='f')&(inStr[2]=='r')) SetVal(DACobj,_Freq_,Parm[0]) ;            // Frequency

        // The next two commands require different default values...
        if (strlen(inStr)==3) Parm[0] = 50 ;                                            // If no value provided, set default to 50
        if ((inStr[1]=='s')&(inStr[2]=='q')) SetVal(DACobj,_Square_,Parm[0]) ;          // Set Square wave   (optional duty cycle parameter)
        if ((inStr[1]=='t')&(inStr[2]=='r')) SetVal(DACobj,_Triangle_,Parm[0]) ;        // Set Triangle wave (optional duty cycle parameter)

        if ((inStr[1] == 's') & (inStr[2] == 'w')) Sweep(DACobj) ;                      // Sweep display mode

        // All successful commands create a ResultStr, so the abscence of a ResultStr indicates an unrecognised command
        if (*_Result.Txt == '\0') {                                                     // Check for empty string
            tmp = strlen(inStr) ;                                                       // Deffo garbage input , but we'll align it nicely regardless
            if (tmp != 0) tmp ++ ;                                                      // Bump to allow for cursor character
            strcpy(_Result.Txt, Margin);
            _Result.Txt[MWidth - tmp] = '\0' ;                                          // Calculate padding for input and cursor
            strcat(_Result.Txt,"Unknown command!\n");
        }

        printf(_Result.Txt) ;                                                           // Update terminal
        SPI_Display_Write(_Result.Val) ;                                                // Update SPI display device

        strcpy(LastCmd, inStr) ;                                                        // Preserve last command
        inStr[0] = '\0' ;                                                               // Reset input string
        outStr[0] = '\0' ;                                                              // Clear (reset) the string variable

        // Serial comms causes phase drift at high frequencies, particularly when using serial over USB.
        // This is mitigated by reseting phase lock after any serial comms activity...
        PhaseLock(DACobj);                                                              // Phase lock the DAC channels
    }
    return 0;
}
