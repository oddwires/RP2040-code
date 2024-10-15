#include "Demo.h"

bool SweepParm (DAC DACobj[], int _parm, int _start, int _stop, int _speed, int _pause) {
    // Returns true when complete, returns false if 'Q' or 'q' pressed.
    char c ;
    int i=_start; 
    int step, count ;
    if (_start<=_stop) { step=1 ;  }                                    // Increment
    else               { step=-1 ; }                                    // Decrement
    while (true) {
        count=0 ;
        i=i+step ;
        if (SelectedChan & 0b01) DACobj[_DAC_A].Set(_parm,i) ;          // Set frequency, display status
        if (SelectedChan & 0b10) DACobj[_DAC_B].Set(_parm,i) ;
        printf(_Result.Txt) ;                                           // Update terminal
        _Result.Txt[0] = '\0' ;                                         // Reset the string variable
        SPI_Display_Write(i) ;
        if ((i==_start)or(i==_stop)) break ;                            // End of sweep
        // Loop to create a short pause between steps of the sweep.
        // Note: keyboard input is still scanned providing immediate exit on key press
        while (count<_speed) {
            c=getchar_timeout_us(0);
            count++;
            sleep_ms(1) ;
            if ((c=='q') or (c=='Q')) {
                while (c!=254) {
                    c=getchar_timeout_us(0) ;                           // wait for key release
                    sleep_ms(1) ;
                }
                return false ;
            }
        }
    }   // Falls through here when sweep has completed.
    // Second loop creates a longer pause at the end of each sweep.
    // Note: keyboard input is still scanned providing immediate exit on key press
    count=0 ;
    while (count<_pause) {
        c=getchar_timeout_us(0);
        count++;
        sleep_ms(1) ;
        if ((c=='q') or (c=='Q')) {
            while (c!=254) {
                c=getchar_timeout_us(0) ;                               // wait for key release
                sleep_ms(1) ;
            }
            return false ;
        }
    }
    return true ;
}

void Demo_01 (DAC DACobj[]) {
// Demo 01: Standard waveforms
//          Sine with varying harmonics
//          Triangle with varying rise times
//          Square wave with varying duty cycle
// Frequency and Level of the Demo can be set through the Function Generator interface.
    int speed=5 ;                                                       // Pause between steps (ms)
    int pause=1000 ;                                                    // Pause between stages (ms)
    int Maxlevel ;                                                      // Max level (amplitude) of the DAC channels
    // Set Maxlevel to the largest existing DAC level. This allows us to use the Function Generator mode to scale the 
    // vertical output to fit the oscilloscope screen. IF we re-use this value, we ensure the demo will also fit the oscilloscope
    // vertical screen size .
    if (DACobj[_DAC_A].Level<DACobj[_DAC_B].Level) Maxlevel=DACobj[_DAC_B].Level ; // Bigger of the two values
    else                                           Maxlevel=DACobj[_DAC_A].Level ;
    SelectedChan=0b011 ;                                                // Select both channels
    SetVal(DACobj,_Level_,0) ;                                          // Set Output level 0
    _Result.Txt[0] = '\0' ;                                             // String also used as a flag, so needs to be cleared
    while (true) {
        SetVal(DACobj,_Triangle_,50) ;                                  // Set Triangle wave, 50% rise time
        if (!SweepParm(DACobj,_Level_,0,Maxlevel,speed,pause)) break ;  // Sweep up          - exit on keypess
        if (!SweepParm(DACobj,_Triangle_,50,0,speed,pause)) break ;     // Rise time -> 0    - exit on keypress
        if (!SweepParm(DACobj,_Triangle_,0,100,speed,pause)) break ;    // Rise time -> 100  - exit on keypress
        if (!SweepParm(DACobj,_Triangle_,100,50,speed,pause)) break ;   // Rise time -> 50   - exit on keypress
        if (!SweepParm(DACobj,_Level_,Maxlevel,0,speed,pause)) break ;  // Sweep down        - exit on keypress
        SetVal(DACobj,_Sine_,0) ;                                       // Set Sine wave, no harmonics
        if (!SweepParm(DACobj,_Level_,0,Maxlevel,speed,pause)) break ;  // Sweep up          - exit on keypess
        if (!SweepParm(DACobj,_Sine_,0,5,pause,pause)) break ;          // Harmonic -> 5     - exit on keypress
        if (!SweepParm(DACobj,_Sine_,5,0,pause,pause)) break ;          // Harmonic -> 0     - exit on keypress
        if (!SweepParm(DACobj,_Level_,Maxlevel,0,speed,pause)) break ;  // Sweep down        - exit on keypress
        SetVal(DACobj,_Square_,50) ;                                    // Set Square wave, 50% duty cycle
        if (!SweepParm(DACobj,_Level_,0,Maxlevel,speed,pause)) break ;  // Sweep up          - exit on keypess
        if (!SweepParm(DACobj,_Square_,50,0,speed,pause)) break ;       // Duty cycle -> 0   - exit on keypress
        if (!SweepParm(DACobj,_Square_,0,100,speed,pause)) break ;      // Duty cycle -> 100 - exit on keypress
        if (!SweepParm(DACobj,_Square_,100,50,speed,pause)) break ;     // Duty cycle -> 50  - exit on keypress
        if (!SweepParm(DACobj,_Level_,Maxlevel,0,speed,pause)) break ;  // Sweep down        - exit on keypress
    }
    sprintf(_Result.Txt,"%sQuit demo #1\n",MarginVW);                   // Prevents error message on return
}

void Demo_02 (DAC DACobj[]) {
// Demo 02: Lazy Lissajous
    int speed=5 ;                                                       // Pause between steps (ms)
    int pause=0 ;                                                       // Pause between stages (ms)
    _Result.Txt[0] = '\0' ;                                             // String also used as a flag, so needs to be cleared
    SelectedChan=0b001 ;                                                // Select channel A
    SetVal(DACobj,_Freq_,100) ;                                         // Set 100Hz
    while (true) {
        SetVal(DACobj,_Sine_,0) ;                                       // Set Sine wave, no harmonics
        SelectedChan=0b010 ;                                            // Select channel B
        SetVal(DACobj,_Freq_,100) ;                                     // Set 100Hz
        if (!SweepParm(DACobj,_Phase_,0,1400,speed,pause)) break ;      // Sweep phase          - exit on keypess
        SetVal(DACobj,_Freq_,200) ;                                     // Set 200Hz
        if (!SweepParm(DACobj,_Phase_,0,1400,speed,pause)) break ;      // Sweep phase          - exit on keypess
        SetVal(DACobj,_Freq_,300) ;                                     // Set 300Hz
        if (!SweepParm(DACobj,_Phase_,0,1400,speed,pause)) break ;      // Sweep phase          - exit on keypess
        SetVal(DACobj,_Freq_,400) ;                                     // Set 400Hz
        if (!SweepParm(DACobj,_Phase_,0,1400,speed,pause)) break ;      // Sweep phase          - exit on keypess
        SetVal(DACobj,_Freq_,500) ;                                     // Set 500Hz
        if (!SweepParm(DACobj,_Phase_,0,1400,speed,pause)) break ;      // Sweep phase          - exit on keypess
        SetVal(DACobj,_Freq_,600) ;                                     // Set 600Hz
        if (!SweepParm(DACobj,_Phase_,0,1400,speed,pause)) break ;      // Sweep phase          - exit on keypess
    }
    sprintf(_Result.Txt,"%sQuit demo #2\n",MarginVW);                   // Prevents error message on return
}