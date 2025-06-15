#pragma once

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "DAC.pio.h"
#include "SPI_Utils.h"

#define BitMapSize     256
#define _Up              1
#define _Down           -1
#define _Sine_           0
#define _Square_         1
#define _Triangle_       2
#define _Freq_           6
#define _Level_          7

extern unsigned short DAC_channel_mask;
extern const uint32_t transfer_count;
extern int MarginCount, SelectedChan, NixieVal;

class DAC {
public:
    PIO pio;
    unsigned short DAC_data[BitMapSize] __attribute__ ((aligned(2048))) ;   // Align DAC data (2048d = 0800h)    
    int Funct, Range, PIOnum, Level, Freq, Phase, DutyC, Harm, RiseT ;
    uint8_t StateMachine, ctrl_chan, data_chan, GPIO, SM_WrapBot, SM_WrapTop ;
    float DAC_div ;
    char name, Suffix[4], RetStr[40] ;

    DAC(char _name, PIO _pio, uint8_t _GPIO);
    char* StatusString();                                               // TBD - is this the same as RetStr ???
    int Set(int _type, int _val);
    int Bump(int _type, int _dirn);
    void DACspeed(int _frequency);
    void DataCalc();

private:
    const float _2Pi = 6.283;                                         // 2*Pi
};

void PhaseLock(DAC DACobj[2]);                                       // Not a member of the class

